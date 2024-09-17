#include <stdbool.h>
#include <stdint.h>

#include <errno.h>
#include <hal/nrf_gpio.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "hid_report_map.h"
#include "hid_report_struct.h"
#include "platform/gpio.h"
#include "platform/spi.h"

LOG_MODULE_REGISTER(avr);

#define DT_DRV_COMPAT atmega_spi_slave

#define CS_PIN      (DT_INST_SPI_DEV_CS_GPIOS_PIN(0))
#define CS_INACTIVE (DT_INST_SPI_DEV_CS_GPIOS_FLAGS(0) & 1)  // bit 1 determines inactive state value (see GPIO_ACTIVE_LOW)
#define IRQ_PIN     (DT_INST_GPIO_PIN(0, irq_gpios))

/**
 * @brief Detect if AVR has appeared (i.e. is powered by USB and is ready to communicate) since last check.
 *
 * AVR's presence is determined by its ability to maintain a specific state of IRQ and CS lines.
 * The IRQ is expected to be high-impedance while CS is low, and should be kept low after CS goes high.
 *
 * This function always leaves IRQ and CS pins in the state depending on whether AVR is present.
 * If it's not present, IRQ is input with no pull-up/pull-down, and CS is low.
 * If the AVR is present, IRQ is an input with pull-up, and CS is high.
 */
static bool did_avr_appear() {
    nrf_gpio_cfg_input(IRQ_PIN, NRF_GPIO_PIN_PULLUP);
    k_usleep(10);

    // When AVR has no power, the AVR becomes ghost-powered through IRQ pin with pull-up resistor enabled.
    // It sinks current, and in most cases this keeps the voltage low enough that it reads as logical 0.
    // Contrary, when AVR is powered from USB bus it has IRQ as high impedance, causing no current leak
    // and thus reads as 1.
    // Sometimes, however, the AVR can be ghost-powered by an ongoing SPI transaction, in which case it will
    // not sink as much current, leaving the input at high level. Therefore to distinguish between the two
    // cases we set CS high so that AVR enables SPI and pulls IRQ low. It can only do so when powered from USB,
    // because ghost powering does not provide enough voltage to run CPU thanks to brown-out protection.
    if (nrf_gpio_pin_read(IRQ_PIN) == 1) {
        nrf_gpio_pin_write(CS_PIN, CS_INACTIVE);
        k_msleep(5);  // give it some time to react
        if (nrf_gpio_pin_read(IRQ_PIN) == 0) {
            return true;
        }
    }

    // reset to original state
    nrf_gpio_pin_write(CS_PIN, 0);
    nrf_gpio_cfg_input(IRQ_PIN, NRF_GPIO_PIN_NOPULL);
    return false;
}

const static struct spi_configuration avr_spi_config = {
    .op_mode = NRF_SPIM_MODE_3,  // same as optical sensor
    .bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST,
    .freq = SPI_CONFIG_FREQUENCY(DT_INST_PROP(0, spi_max_frequency)),
    .is_const = true,
};

static void avr_init_gpios() {
    // can't keep CS always high because this would ghost-power the AVR
    // instead it is only set high when the AVR is bus-powered
    nrf_gpio_pin_write(CS_PIN, 0);
    nrf_gpio_cfg_output(CS_PIN);

    // similarly, the pull-up is not enabled constantly to avoid ghost-powering
    nrf_gpio_cfg_input(IRQ_PIN, NRF_GPIO_PIN_NOPULL);
}

/**
 * @brief Update CRC value with the new data byte
 *
 * Adapted from C implementation of @c _crc_ibutton_update from avr-gcc.
 */
static uint8_t update_crc(uint8_t crc, uint8_t data) {
    crc = crc ^ data;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x01)
            crc = (crc >> 1) ^ 0x8C;
        else
            crc >>= 1;
    }
    return crc;
}

/**
 * @brief Perform SPI transfer and validate CRC of received data.
 */
static int avr_transceive(struct spi_transfer_spec* spec) {
    uint32_t rx_len = spec->rx_len;
    uint8_t* rx_buf = spec->rx_buf;

    int err = spi_transceive_tx_then_rx(spec, &avr_spi_config, CS_PIN);
    if (err) {
        LOG_ERR("failed to transceive: %d", err);
        return err;
    }
    if (rx_len) {
        uint8_t crc = 0;
        for (uint32_t i = 0; i < rx_len - 1; i++) {
            crc = update_crc(crc, rx_buf[i]);
        }
        if (crc != rx_buf[rx_len-1]) {
            LOG_WRN("expected crc 0x%x, got 0x%x (payload size %u)", crc, rx_buf[rx_len-1], rx_len);
            return -EIO;
        }
    }
    return 0;
}

/**
 * @brief Verify AVR ID via command @c 0x06.
 *
 * Returns negative error code on error, 0 if AVR responds with expected ID and 1 if the ID is incorrect.
 */
static int verify_avr_id(bool log_err) {
    uint8_t request = 0x06;
    uint8_t response[4];
    struct spi_transfer_spec spec = {&request, 1, response, sizeof(response)};
    int err = avr_transceive(&spec);
    if (err) {
        return err;
    }
    if (response[0] != 0x1E || response[1] != 0x93 || response[2] != 0x89) {
        if (log_err) {
            LOG_ERR("invalid response id: %x %x %x", response[0], response[1], response[2]);
        }
        return 1;
    }
    return 0;
}

static int send_report_descriptor() {
    uint8_t request[HID_REPORT_MAP_SIZE + 1];
    request[0] = 0x08; // send report descriptor
    memcpy(request + 1, hid_report_map, HID_REPORT_MAP_SIZE);
    struct spi_transfer_spec spec = {request, sizeof(request), NULL, 0};
    return avr_transceive(&spec);
}

static int enable_usb() {
    uint8_t request[] = {0x09, 0x01};  // command id, enable usb
    struct spi_transfer_spec spec = {request, sizeof(request), NULL, 0};
    return avr_transceive(&spec);
}

static int send_report() {
    struct hid_report report = {.x_delta = 1};
    uint8_t request[sizeof(report) + 2];  // command + report id + report
    request[0] = 0x0A; // send report descriptor
    request[1] = HID_REPORT_ID;
    memcpy(request + 2, &report, sizeof(report));
    struct spi_transfer_spec spec = {request, sizeof(request), NULL, 0};
    return avr_transceive(&spec);
}

static void avr_comm_loop() {
    k_usleep(150);
    send_report_descriptor();
    k_usleep(150);
    enable_usb();
    do {
        k_usleep(150);
        send_report();
        k_msleep(10);
    } while (verify_avr_id(false) != 1);
}

static int avr_communication_thread(void* p1, void* p2, void* p3) {
    avr_init_gpios();

    while (true) {
        while (!did_avr_appear()) {
            k_msleep(500);
        }
        LOG_INF("detected avr");
        k_msleep(1);
        if (verify_avr_id(true)) {
            LOG_ERR("failed to verify device ID");
            // In this case we can't really set CS low, because this might trigger something in AVR
            // while talking to optical sensor. Best thing we can do is to wait until it releases IRQ,
            // which it can only do on powerdown (if this is ever gonna happen...)
            while (nrf_gpio_pin_read(IRQ_PIN) == 0) {
                k_msleep(100);
            }
        } else {
            avr_comm_loop();
        }
        LOG_INF("avr gone");
        avr_init_gpios();  // restore CS and IRQ to inactive state
    }

    return 0;
}

K_THREAD_DEFINE(avr_comm,
                CONFIG_APP_AVR_COMM_THREAD_STACK_SIZE,
                avr_communication_thread,
                NULL, NULL, NULL,
                CONFIG_APP_AVR_COMM_THREAD_PRIORITY,
                0,
                0);

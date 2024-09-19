#include "drivers/adns7530.h"

#include <stdbool.h>
#include <stdint.h>

#include <errno.h>
#include <hal/nrf_gpio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "platform/spi.h"

LOG_MODULE_REGISTER(adns7530);

#ifdef CONFIG_SOC_NRF52832
// when SPIM is used, transactions where RX buffer size is 1 will trigger PAN 58
// receive 2 bytes explicitly instead, sensor won't do anything during 2nd byte clk
#define ONE_BYTE_RX_BUF_SIZE 2
#else
#define ONE_BYTE_RX_BUF_SIZE 1
#endif

#define CS_PIN      (DT_INST_SPI_DEV_CS_GPIOS_PIN(0))
#define CS_INACTIVE (DT_INST_SPI_DEV_CS_GPIOS_FLAGS(0) & 1)  // bit 1 determines inactive state value (see GPIO_ACTIVE_LOW)

const static struct spi_configuration adns7530_spi_config = {
    .op_mode = ADNS7530_SPI_MODE,
    .bit_order = ADNS7530_SPI_BITORD,
    .freq = SPI_CONFIG_FREQUENCY(DT_INST_PROP(0, spi_max_frequency)),
    .is_const = true,
};

static struct {
    int err;
    int rx_len;
    void* rx_buf;
} adns7530_spi_cb_ctx;

static void adns7530_spi_done_callback(void* arg) {
    if (adns7530_spi_cb_ctx.rx_buf) {
        // the SPI task/event latency is about 6 us in total, and rescheduling another transaction takes ~5us
        // the total latency is more than required delay t_{SRAD} = 4us, so no additional delay needed
        struct spi_transfer_spec rx_spec = {NULL, 0, adns7530_spi_cb_ctx.rx_buf, adns7530_spi_cb_ctx.rx_len};
        adns7530_spi_cb_ctx.rx_buf = NULL;
        adns7530_spi_cb_ctx.err = spi_transceive(&rx_spec, adns7530_spi_done_callback, arg);
        if (!adns7530_spi_cb_ctx.err) {
            return;  // wait for callback from second transaction
        }
    }
    k_sem_give((struct k_sem*)arg);
}

static int adns7530_spi_transceive(void* tx_buf, uint32_t tx_len, void* rx_buf, uint32_t rx_len) {
    struct spi_transfer_spec tx_spec = {tx_buf, tx_len, NULL, 0};
    adns7530_spi_cb_ctx.err = -EIO;  // will remain unchanged if the callback is never called
    adns7530_spi_cb_ctx.rx_len = rx_len;
    adns7530_spi_cb_ctx.rx_buf = rx_buf;
    int err = spi_transceive_managed(&adns7530_spi_config, CS_PIN, &tx_spec, adns7530_spi_done_callback, K_USEC(1000));
    return err ? err : adns7530_spi_cb_ctx.err;
}

static inline int adns7530_reg_read(uint8_t reg, void* dst, uint32_t count) {
    uint8_t addr = reg & 0x7f;
    uint8_t dst_dummy_buf[count];
    if (dst == NULL) {
        dst = dst_dummy_buf;
    }

    return adns7530_spi_transceive(&addr, 1, dst, count);
}

static inline int adns7530_reg_write(uint8_t reg, uint8_t value) {
    static uint8_t tx_buf[2];
    tx_buf[0] = 0x80 | (reg & 0x7f);
    tx_buf[1] = value;

    return adns7530_spi_transceive(tx_buf, 2, NULL, 0);
}

static int adns7530_init(const struct device *dev) {
    nrf_gpio_pin_write(CS_PIN, CS_INACTIVE);
    nrf_gpio_cfg_output(CS_PIN);

    adns7530_reg_write(ADNS7530_REG_POWER_UP_RESET, ADNS7530_RESET_VALUE);
    k_msleep(10);
    adns7530_reg_write(ADNS7530_REG_OBSERVATION, 0x00);
    k_msleep(10);
    uint8_t observation_val[ONE_BYTE_RX_BUF_SIZE] = {};
    adns7530_reg_read(ADNS7530_REG_OBSERVATION, observation_val, ONE_BYTE_RX_BUF_SIZE);
    if ((*observation_val & 0xF) != 0xF) {
        LOG_ERR("invalid observation value %x", *observation_val);
        return -ENODATA;
    }
    adns7530_reg_read(ADNS7530_REG_MOTION_BURST, NULL, 4);

    // Init sequence as specified in datasheet, meaning of all these registers is undocumented
    adns7530_reg_write(0x3C, 0x27);
    adns7530_reg_write(0x22, 0x0A);
    adns7530_reg_write(0x21, 0x01);
    adns7530_reg_write(0x3C, 0x32);
    adns7530_reg_write(0x23, 0x20);
    adns7530_reg_write(0x3C, 0x05);
    adns7530_reg_write(0x37, 0xB9);

    // Verify product ID
    uint8_t product_id[ONE_BYTE_RX_BUF_SIZE] = {};
    adns7530_reg_read(ADNS7530_REG_PRODUCT_ID, product_id, ONE_BYTE_RX_BUF_SIZE);
    if (*product_id != ADNS7530_PRODUCT_ID) {
        LOG_ERR("invalid product id %x", *product_id);
        return -ENODATA;
    }

    // Settings
    adns7530_reg_write(ADNS7530_REG_LASER_CTRL0, 0x00);
    adns7530_reg_write(ADNS7530_REG_LASER_CTRL1, 0xC0);
    adns7530_reg_write(ADNS7530_REG_LSRPWR_CFG0, 0xE0);
    adns7530_reg_write(ADNS7530_REG_LSRPWR_CFG1, 0x1F);

    // Resolution and sleep mode timings
    adns7530_reg_write(ADNS7530_REG_CFG, ADNS7530_RESOLUTION_1200 | ADNS7530_REST_ENABLE | ADNS7530_RUN_RATE_4MS);
    adns7530_reg_write(ADNS7530_REG_REST2_DOWNSHIFT, 0x0A);
    adns7530_reg_write(ADNS7530_REG_REST3_RATE, 0x63);

    return 0;
}

static int adns7530_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    struct adns7530_data* data = dev->data;
    struct {
        uint8_t motion;
        uint8_t delta_x_l;
        uint8_t delta_y_l;
        uint8_t delta_xy_h;
        uint8_t surf_qual;
    } motion_burst = {};

    adns7530_reg_read(ADNS7530_REG_MOTION_BURST, &motion_burst, sizeof(motion_burst));

    if (motion_burst.motion & ADNS7530_LASER_FAULT_MASK || !(motion_burst.motion & ADNS7530_LASER_CFG_VALID_MASK)) {
        LOG_ERR("laser fault or laser invalid cfg: %x", motion_burst.motion);
        return -ENODATA;
    }

    // ADNS7530_MOTION_FLAG probably means "data ready" rather than "motion detected"
    if (!(motion_burst.motion & ADNS7530_MOTION_FLAG) || motion_burst.surf_qual < CONFIG_ADNS7530_SURF_QUAL_THRESHOLD) {
        data->delta_x = data->delta_y = 0;
        return 0;
    }

    data->delta_x = motion_burst.delta_x_l | ((motion_burst.delta_xy_h & 0xF0) << 4);
    data->delta_y = motion_burst.delta_y_l | ((motion_burst.delta_xy_h & 0x0F) << 8);

    // data is 12-bit signed int
    if (data->delta_x >> 11) data->delta_x = (data->delta_x & 0x7FF) - 0x800;
    if (data->delta_y >> 11) data->delta_y = (data->delta_y & 0x7FF) - 0x800;

    return 0;
}

static int adns7530_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val) {
    struct adns7530_data* data = dev->data;

    switch (chan) {
        case SENSOR_CHAN_POS_DX:
            val->val1 = data->delta_x;
            break;
        case SENSOR_CHAN_POS_DY:
            val->val1 = data->delta_y;
            break;
        default:
            return -ENOTSUP;
    }

    return 0;
}

static struct adns7530_data adns7530_data = { };

static const struct sensor_driver_api adns7530_api_funcs = {
    .sample_fetch = adns7530_sample_fetch,
    .channel_get = adns7530_channel_get,
};

DEVICE_DT_INST_DEFINE(
    0,
    adns7530_init,
    NULL,
    &adns7530_data,
    NULL,
    POST_KERNEL,
    CONFIG_SENSOR_INIT_PRIORITY,
    &adns7530_api_funcs);

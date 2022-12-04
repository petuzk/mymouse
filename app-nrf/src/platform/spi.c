#include "platform/spi.h"

#include <devicetree.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_spim.h>
#include <init.h>
#include <logging/log.h>
#include <kernel.h>

LOG_MODULE_REGISTER(spi);

#define SPIM_NODE DT_NODELABEL(spi0)
#define SCK_PIN   DT_PROP(SPIM_NODE, sck_pin)
#define MOSI_PIN  DT_PROP(SPIM_NODE, mosi_pin)
#define MISO_PIN  DT_PROP(SPIM_NODE, miso_pin)

K_SEM_DEFINE(spi_sem, 1, 1);

struct {
    void (*callback)();
} spi_ctx = {};

static void spi_irq_handler() {
    // irq is called on END event only
    nrf_spim_event_clear(NRF_SPIM0, NRF_SPIM_EVENT_END);
    // give sem before calling callback so that it can start new xfer
    k_sem_give(&spi_sem);
    if (spi_ctx.callback) {
        spi_ctx.callback();
    }
}

static int spi_init(const struct device* dev) {
    ARG_UNUSED(dev);

    nrf_gpio_pin_clear(SCK_PIN);
    nrf_gpio_pin_clear(MOSI_PIN);

    nrf_gpio_cfg_output(SCK_PIN);
    nrf_gpio_cfg_output(MOSI_PIN);
    nrf_gpio_cfg_input(MISO_PIN, NRF_GPIO_PIN_NOPULL);

    nrf_spim_pins_set(NRF_SPIM0, SCK_PIN, MOSI_PIN, MISO_PIN);
    nrf_spim_orc_set(NRF_SPIM0, 0x00);

    nrf_spim_enable(NRF_SPIM0);

    nrf_spim_int_enable(NRF_SPIM0, NRF_SPIM_INT_END_MASK);
    IRQ_CONNECT(DT_IRQN(SPIM_NODE), DT_IRQ(SPIM_NODE, priority), spi_irq_handler, NULL, 0);
    irq_enable(DT_IRQN(SPIM_NODE));

    return 0;
}

static inline bool is_addr_in_ram(const void* ptr)
{
    return ((((uint32_t)ptr) & 0xE0000000u) == 0x20000000u);
}

int spi_transceive(void* tx_buf, uint32_t tx_len, void* rx_buf, uint32_t rx_len, void (*callback)()) {
    if ((tx_buf != NULL && !is_addr_in_ram(tx_buf)) || (rx_buf != NULL && !is_addr_in_ram(rx_buf))) {
        LOG_ERR("buf not in ram");
        return -EINVAL;
    }

    if (k_sem_take(&spi_sem, K_NO_WAIT) != 0) {
        LOG_ERR("busy");
        return -EBUSY;
    }

    spi_ctx.callback = callback;

    nrf_spim_tx_buffer_set(NRF_SPIM0, tx_buf, tx_len);
    nrf_spim_rx_buffer_set(NRF_SPIM0, rx_buf, rx_len);
    nrf_spim_event_clear(NRF_SPIM0, NRF_SPIM_EVENT_END);
    nrf_spim_task_trigger(NRF_SPIM0, NRF_SPIM_TASK_START);

    return 0;
}

SYS_INIT(spi_init, POST_KERNEL, CONFIG_PLATFORM_INIT_PRIORITY);

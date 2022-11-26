#include "platform/spi.h"

#include <devicetree.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_spim.h>
#include <init.h>
#include <logging/log.h>
#include <kernel.h>

LOG_MODULE_REGISTER(spi);

#define SPIM_NODE   DT_NODELABEL(spi0)
#define SCK_PIN     DT_PROP(SPIM_NODE, sck_pin)
#define MOSI_PIN    DT_PROP(SPIM_NODE, mosi_pin)
#define MISO_PIN    DT_PROP(SPIM_NODE, miso_pin)
#define NUM_CS_PINS DT_PROP_LEN(SPIM_NODE, cs_gpios)

// bit 1 determines inactive state value (see GPIO_ACTIVE_LOW)
// GPIO_ACTIVE_LOW comes from gpio.h, which can't be included since we are resigning from GPIO driver
#define GPIO_INACTIVE_MASK 1

#define init_cs_gpio(node_id, gpio_pha, idx) \
    nrf_gpio_pin_write(DT_GPIO_PIN_BY_IDX(node_id, gpio_pha, idx), DT_GPIO_FLAGS_BY_IDX(node_id, gpio_pha, idx) & GPIO_INACTIVE_MASK); \
    nrf_gpio_cfg_output(DT_GPIO_PIN_BY_IDX(node_id, gpio_pha, idx));

#define init_cs_gpios() do {DT_FOREACH_PROP_ELEM(SPIM_NODE, cs_gpios, init_cs_gpio)} while (0)

K_SEM_DEFINE(spi_sem, 0, 1);

struct {
    void (*callback)();
    int cs_pin;
    int cs_inactive;
} spi_ctx = {};

static void spi_irq_handler() {
    // irq is called on END event only
    nrf_spim_event_clear(NRF_SPIM0, NRF_SPIM_EVENT_END);
    nrf_gpio_pin_write(spi_ctx.cs_pin, spi_ctx.cs_inactive);
    k_sem_give(&spi_sem); // give sem before calling callback so that it can start new xfer
    if (spi_ctx.callback) {
        spi_ctx.callback();
    }
}

static int spi_init(const struct device* dev) {
    ARG_UNUSED(dev);

    init_cs_gpios();

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

int spi_transceive(int cs_pin, int cs_inactive, void* tx_buf, uint32_t tx_len, void* rx_buf, uint32_t rx_len) {
    if ((tx_buf != NULL && !is_addr_in_ram(tx_buf)) || (rx_buf != NULL && !is_addr_in_ram(rx_buf))) {
        LOG_ERR("buf not in ram");
        return -EINVAL;
    }

    if (k_sem_take(&spi_sem, K_NO_WAIT) != 0) {
        return -EBUSY;
    }

    spi_ctx.cs_pin = cs_pin;
    spi_ctx.cs_inactive = cs_inactive;
    nrf_gpio_pin_write(cs_pin, !cs_inactive);

    nrf_spim_tx_buffer_set(NRF_SPIM0, tx_buf, tx_len);
    nrf_spim_rx_buffer_set(NRF_SPIM0, rx_buf, rx_len);
    nrf_spim_event_clear(NRF_SPIM0, NRF_SPIM_EVENT_END);
    nrf_spim_task_trigger(NRF_SPIM0, NRF_SPIM_TASK_START);

    return 0;
}

SYS_INIT(spi_init, POST_KERNEL, CONFIG_PLATFORM_INIT_PRIORITY);

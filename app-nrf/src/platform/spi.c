#include "platform/spi.h"

#include <stdbool.h>
#include <stdint.h>

#include <hal/nrf_gpio.h>
#include <hal/nrf_spim.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(spi);

#define SPIM_NODE DT_NODELABEL(spi0)
#define SCK_PIN   DT_PROP(SPIM_NODE, sck_pin)
#define MOSI_PIN  DT_PROP(SPIM_NODE, mosi_pin)
#define MISO_PIN  DT_PROP(SPIM_NODE, miso_pin)

struct {
    void (*callback)();
} spi_isr_ctx = {};

K_MUTEX_DEFINE(spi_mutex);
static bool is_in_spi_isr = false;

static const struct spi_configuration* prev_config = NULL;

// truth table:
//   ISR  |  SPI ISR  |   owner   |  error
//    y   |     y     |     -     |    n
//    y   |     n     |     -     |    y
//    n   |     -     |  current  |    n
//    n   |     -     |   other   |    y

#define CHECK_LOCK_OWNED() if (k_is_in_isr() ? !is_in_spi_isr : k_current_get() != spi_mutex.owner) return -EPERM

#if CONFIG_SPI_DISABLE_CLK_DELAY > 0
static inline bool is_sck_inactive_high(nrf_spim_mode_t mode) {
    return mode == NRF_SPIM_MODE_2 || mode == NRF_SPIM_MODE_3;
}

static void disable_sck(struct k_work *work) {
    nrf_spim_configure(NRF_SPIM0, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
    prev_config = NULL;
}

K_WORK_DELAYABLE_DEFINE(disable_sck_work, disable_sck);
#endif // CONFIG_SPI_DISABLE_CLK_DELAY > 0

int spi_lock(k_timeout_t timeout) {
#if CONFIG_SPI_DISABLE_CLK_DELAY > 0
    k_work_cancel_delayable(&disable_sck_work);
#endif
    return k_mutex_lock(&spi_mutex, timeout);
}

int spi_unlock() {
    // k_mutex_unlock returns an error if the lock is not owned by this thread, no need to CHECK_LOCK_OWNED
    int err = k_mutex_unlock(&spi_mutex);
#if CONFIG_SPI_DISABLE_CLK_DELAY > 0
    if (likely(!err) && prev_config && is_sck_inactive_high(prev_config->op_mode)) {
        // if previous configuration has SCK inactive high, schedule disabling it after a period of inactivity
        // this is needed to prevent AVR ghost powering via SCK line
        k_work_schedule(&disable_sck_work, K_USEC(CONFIG_SPI_DISABLE_CLK_DELAY));
    }
#endif
    return err;
}

int spi_configure(const struct spi_configuration* config) {
    CHECK_LOCK_OWNED();

    if (config->is_const && config == prev_config) {
        return 0;
    }

    nrf_spim_configure(NRF_SPIM0, config->op_mode, config->bit_order);
    nrf_spim_frequency_set(NRF_SPIM0, config->freq);

#if CONFIG_SPI_ENABLE_CLK_DELAY > 0
    if ((!prev_config || !is_sck_inactive_high(prev_config->op_mode)) && is_sck_inactive_high(config->op_mode)) {
        // wait more time than normally so that AVR's capacitors can charge up from SCK
        k_usleep(CONFIG_SPI_ENABLE_CLK_DELAY);
    }
#endif

    prev_config = config;

    return 0;
}

static inline bool is_addr_in_ram(const void* ptr)
{
    return ((((uint32_t)ptr) & 0xE0000000u) == 0x20000000u);
}

int spi_transceive(const struct spi_transfer_spec* spec, void (*callback)()) {
    CHECK_LOCK_OWNED();

    if ((spec->tx_buf != NULL && !is_addr_in_ram(spec->tx_buf)) || (spec->rx_buf != NULL && !is_addr_in_ram(spec->rx_buf))) {
        LOG_ERR("buf not in ram");
        return -EINVAL;
    }

    spi_isr_ctx.callback = callback;

    nrf_spim_tx_buffer_set(NRF_SPIM0, spec->tx_buf, spec->tx_len);
    nrf_spim_rx_buffer_set(NRF_SPIM0, spec->rx_buf, spec->rx_len);
    nrf_spim_event_clear(NRF_SPIM0, NRF_SPIM_EVENT_END);
    nrf_spim_task_trigger(NRF_SPIM0, NRF_SPIM_TASK_START);

    return 0;
}

K_SEM_DEFINE(spi_sync_sem, 0, 1);

static void spi_transceive_sync_callback() {
    k_sem_give(&spi_sync_sem);
}

int spi_transceive_sync(struct spi_transfer_spec* spec) {
    int err = spi_transceive(spec, spi_transceive_sync_callback);
    if (err) {
        return err;
    }
    return k_sem_take(&spi_sync_sem, K_FOREVER);
}

static void spi_irq_handler() {
    // irq is called on END event only
    nrf_spim_event_clear(NRF_SPIM0, NRF_SPIM_EVENT_END);
    if (spi_isr_ctx.callback) {
        is_in_spi_isr = true;
        spi_isr_ctx.callback();
        is_in_spi_isr = false;
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

SYS_INIT(spi_init, POST_KERNEL, CONFIG_PLATFORM_INIT_PRIORITY);

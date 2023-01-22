#include "platform/qdec.h"

#include <hal/nrf_gpio.h>
#include <hal/nrf_qdec.h>
#include <zephyr/init.h>

#define QDEC_DT_NODE       DT_NODELABEL(rotary_encoder)
#define QDEC_PIN(pin_name) DT_PROP(QDEC_DT_NODE, pin_name ## _pin)
#define A_PIN QDEC_PIN(a)
#define B_PIN QDEC_PIN(b)

static qdec_data_cb callback = NULL;

void qdec_set_data_cb(qdec_data_cb cb) {
    callback = cb;
}

static void qdec_irq_handler() {
    // irq is called on REPORTRDY event only
    nrf_qdec_event_clear(NRF_QDEC, NRF_QDEC_EVENT_REPORTRDY);

    if (callback) {
        callback(nrf_qdec_accread_get(NRF_QDEC));
    }
}

static int qdec_init(const struct device *dev) {
    ARG_UNUSED(dev);

    // configure gpios
    nrf_gpio_cfg_input(A_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(B_PIN, NRF_GPIO_PIN_PULLUP);

    // stop qdec (maybe we're doing a software reset?)
    nrf_qdec_task_trigger(NRF_QDEC, NRF_QDEC_TASK_STOP);

    // configure qdec
    nrf_qdec_pins_set(NRF_QDEC, A_PIN, B_PIN, NRF_QDEC_LED_NOT_CONNECTED);
    nrf_qdec_sampleper_set(NRF_QDEC, NRF_QDEC_SAMPLEPER_256us);
    nrf_qdec_reportper_set(NRF_QDEC, NRF_QDEC_REPORTPER_1);
    nrf_qdec_shorts_enable(NRF_QDEC, NRF_QDEC_SHORT_REPORTRDY_READCLRACC_MASK);
    nrf_qdec_dbfen_disable(NRF_QDEC);

    // configure interrupt
    nrf_qdec_event_clear(NRF_QDEC, NRF_QDEC_EVENT_REPORTRDY);
    nrf_qdec_int_enable(NRF_QDEC, NRF_QDEC_INT_REPORTRDY_MASK);
    IRQ_CONNECT(DT_IRQN(QDEC_DT_NODE), DT_IRQ(QDEC_DT_NODE, priority), qdec_irq_handler, NULL, 0);
    irq_enable(DT_IRQN(QDEC_DT_NODE));

    // enable and start qdec
    nrf_qdec_enable(NRF_QDEC);
    nrf_qdec_task_trigger(NRF_QDEC, NRF_QDEC_TASK_START);

    return 0;
}

SYS_INIT(qdec_init, POST_KERNEL, CONFIG_PLATFORM_INIT_PRIORITY);

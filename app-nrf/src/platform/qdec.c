#include "platform/qdec.h"

#include <hal/nrf_gpio.h>
#include <hal/nrf_qdec.h>
#include <init.h>

#define QDEC_PIN(pin_name) DT_PROP(DT_NODELABEL(rotary_encoder), pin_name ## _pin)
#define A_PIN QDEC_PIN(a)
#define B_PIN QDEC_PIN(b)

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
    nrf_qdec_dbfen_disable(NRF_QDEC);

    // enable and start qdec
    nrf_qdec_enable(NRF_QDEC);
    nrf_qdec_task_trigger(NRF_QDEC, NRF_QDEC_TASK_START);

    return 0;
}

int qdec_read_acc() {
    nrf_qdec_task_trigger(NRF_QDEC, NRF_QDEC_TASK_READCLRACC);
    return nrf_qdec_accread_get(NRF_QDEC);
}

SYS_INIT(qdec_init, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY);

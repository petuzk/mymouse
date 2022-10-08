#include "platform/gpio.h"

#include <stdint.h>

#include <hal/nrf_gpio.h>
#include <init.h>

const uint32_t button_inputs[] = {
    FOR_EACH(PINOF, (,),
        button_left,
        button_right,
        button_middle,
        button_spec,
        button_center,
        button_up,
        button_down,
        button_fwd,
        button_bwd),
};

const uint32_t outputs[] = {
    FOR_EACH(PINOF, (,),
        led_green,
        led_red),
};

static int gpio_init(const struct device *dev) {
    ARG_UNUSED(dev);

    for (int i = 0; i < ARRAY_SIZE(button_inputs); i++) {
        nrf_gpio_cfg_input(button_inputs[i], NRF_GPIO_PIN_PULLUP);
    }

    for (int i = 0; i < ARRAY_SIZE(outputs); i++) {
        nrf_gpio_pin_clear(outputs[i]);
        nrf_gpio_cfg_output(outputs[i]);
    }

    return 0;
}

SYS_INIT(gpio_init, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY);

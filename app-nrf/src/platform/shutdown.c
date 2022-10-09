#include "platform/shutdown.h"

#include <hal/nrf_gpio.h>
#include <hal/nrf_power.h>
#include <hal/nrf_spim.h>

#include "platform/gpio.h"

void shutdown() {
    // disable vdd_pwr by setting pin high
    nrf_gpio_pin_set(PINOFPROP(vdd_pwr, enable_gpios));

    // enable sense for button_mode pin for wake up
    nrf_gpio_cfg_sense_input(PINOF(button_mode), NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);

    // disable spi
    nrf_spim_disable(NRF_SPIM0);

    // disconnect all other pins
    for (int pin = 0; pin < 32; pin++) {
        if (pin == PINOFPROP(vdd_pwr, enable_gpios) || pin == PINOF(button_mode)) {
            continue;
        }
        nrf_gpio_cfg_default(pin);
    }

    // enter poweroff
    nrf_power_system_off(NRF_POWER);

    CODE_UNREACHABLE;
}

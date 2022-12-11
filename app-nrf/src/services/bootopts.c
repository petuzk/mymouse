#include "services/bootopts.h"

#include <hal/nrf_gpio.h>
#include <zephyr/init.h>

#include "platform/gpio.h"

struct bootopts boot_options = {};

static int detect_bootopts(const struct device *dev) {
    ARG_UNUSED(dev);

    boot_options.bt_public_adv = !nrf_gpio_pin_read(PINOF(button_spec));
    return 0;
}

SYS_INIT(detect_bootopts, APPLICATION, CONFIG_APP_BOOTOPTS_INIT_PRIORITY);

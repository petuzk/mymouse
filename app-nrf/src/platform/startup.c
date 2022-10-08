#include <hal/nrf_gpio.h>
#include <hal/nrf_timer.h>
#include <init.h>

#include "platform/gpio.h"
#include "platform/hw_resources.h"
#include "platform/qdec.h"
#include "platform/shutdown.h"
#include "platform/timer_utils.h"

#define BUTTON_MODE_PIN       PINOF(button_mode)
#define VDD_PWR_PIN           PINOFPROP(vdd_pwr, enable_gpios)

#ifndef CONFIG_SKIP_BOOT_CONDITION
#define BUTTON_WAIT_TIME (CONFIG_POWER_ON_OFF_HOLD_TIME * 1000 / 32)
#else
#define BUTTON_WAIT_TIME 0
#endif

static int platform_init_check_boot_cond(const struct device *dev) {
    ARG_UNUSED(dev);

    /* timer configuration:
     *   - frequency: 31250 Hz
     *   - channels:
     *       0: used to count time for which button has to be held
     *       1: extra ~1 ms to ensure stable Vdd, checked in second init step
     *   - timer is automatically stopped on channel #1 compare
     */

    timer_reconfigure(ON_OFF_DELAY_TIMER, NRF_TIMER_FREQ_31250Hz);
    timer_set_cc(ON_OFF_DELAY_TIMER, 0, BUTTON_WAIT_TIME);
    timer_set_cc(ON_OFF_DELAY_TIMER, 1, BUTTON_WAIT_TIME + 32);
    timer_start(ON_OFF_DELAY_TIMER);
    nrf_timer_shorts_set(ON_OFF_DELAY_TIMER, NRF_TIMER_SHORT_COMPARE1_STOP_MASK);

    // disable regulator until enabled after boot cond check
    // do it before setting pin as output to make it "default value"
    nrf_gpio_pin_set(VDD_PWR_PIN);

    // configure gpio directions
    nrf_gpio_cfg_output(VDD_PWR_PIN);
    nrf_gpio_cfg_input(BUTTON_MODE_PIN, NRF_GPIO_PIN_PULLUP);

#ifndef CONFIG_SKIP_BOOT_CONDITION
    while (!timer_expired(ON_OFF_DELAY_TIMER, 0)) {
        if (nrf_gpio_pin_read(BUTTON_MODE_PIN)) {
            // button released while timer not yet expired
            shutdown();
        }
    }
#endif

    // enable vdd pwr by clearing output (it's active low)
    nrf_gpio_pin_clear(VDD_PWR_PIN);
    return 0;
}

static int platform_init_ensure_vdd_stable(const struct device *dev) {
    // wait for vdd to stabilize using timer started in platform_init_check_boot_cond
    while (!timer_expired(ON_OFF_DELAY_TIMER, 1));
    return 0;
}

// should be after mpsl_lib_init (/opt/nordic/ncs/v1.7.1/nrf/subsys/mpsl/init/mpsl_init.c:211)
SYS_INIT(platform_init_check_boot_cond, PRE_KERNEL_1, CONFIG_PLATFORM_INIT_PRIORITY);

// ensure vdd is stable as a last step of kernel init
// assuming that external devices init may take place at least at POST_KERNEL
SYS_INIT(platform_init_ensure_vdd_stable, PRE_KERNEL_2, 99);

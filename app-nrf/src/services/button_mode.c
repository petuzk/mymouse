#include "services/button_mode.h"

#include <hal/nrf_gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "platform/gpio.h"
#include "platform/pwm.h"
#include "platform/shutdown.h"
#include "services/battery.h"

#define SHORT_FADE_MS 60
#define LONG_FADE_MS  500

LOG_MODULE_REGISTER(button_mode);

enum lifecycle_state {
    freshly_booted,
    normal_operation,
    waiting_for_shutdown,
};

static enum lifecycle_state lifecycle_state = freshly_booted;

static void lifecycle_timer_cb(struct k_timer *timer) {
    if (lifecycle_state == freshly_booted) {
        // freshly booted state means this timer is used to schedule led fade off
        // it is done once, so change state after that
        pwm_schedule_sequence_from_last((struct led_value) {0, 0}, LONG_FADE_MS);
        lifecycle_state = normal_operation;
    } else {
        // otherwise this timer is used to wait for button being held
        // if it is held long enough, shutdown must be performed
        // here we indicate that shutdown is ready, actual shutdown is performed on button release
        pwm_schedule_sequence_from_zero((struct led_value) {80, 0}, SHORT_FADE_MS);
        lifecycle_state = waiting_for_shutdown;
    }
}

K_TIMER_DEFINE(lifecycle_timer, lifecycle_timer_cb, NULL);

void check_button_mode() {
    static int prev_btn_state = 0;
    int btn_state = nrf_gpio_pin_read(PINOF(button_mode));

    // if button state did not change, nothing to do
    if (btn_state == prev_btn_state) {
        return;
    }

    prev_btn_state = btn_state;

    if (lifecycle_state == freshly_booted) {
        int timer_running = k_timer_remaining_get(&lifecycle_timer);
        if (btn_state == 1 && !timer_running) {
            // button released, schedule led fade off if not scheduled yet
            k_timer_start(&lifecycle_timer, K_SECONDS(3), K_NO_WAIT);
        }
    }
    else if (lifecycle_state == normal_operation) {
        if (btn_state == 0) {
            // button pressed, shutdown will be triggered
            // unless timer will be stopped before timeout by button release
            k_timer_start(&lifecycle_timer, K_MSEC(CONFIG_POWER_ON_OFF_HOLD_TIME), K_NO_WAIT);
        } else {
            // button released before timeout, stop timer
            k_timer_stop(&lifecycle_timer);
        }
    }
    else if (lifecycle_state == waiting_for_shutdown) {
        // button released after shutdown ready indication, shutdown
        pwm_schedule_sequence_from_last((struct led_value) {0, 0}, SHORT_FADE_MS);
        k_msleep(SHORT_FADE_MS); // wait for led to turn off and debounce to not trigger boot immediately
        shutdown();
    }
}

static int button_mode_init(const struct device* dev) {
    ARG_UNUSED(dev);

    while (battery_latest_measure_time_ms() == -1) {
        // in case battery measurement is not yet ready, wait for it
        // sleep value is arbitrary, but tested to be long enough to only call sleep once
        k_usleep(10);
    }

    struct led_value led_col;
    int bat_level = battery_charge_level();

    if (bat_level > 50) {
        led_col.r = (100 - bat_level) * 2;  // range from 0 to 98 both incl, ok for approx
        led_col.g = 99;
    } else {
        led_col.r = 99;
        led_col.g = bat_level * 2;  // range from 0 to 100, out of range, but should be ok for pwm hw
    }

    pwm_schedule_sequence_from_zero(led_col, SHORT_FADE_MS);

    return 0;
}

SYS_INIT(button_mode_init, APPLICATION, CONFIG_APP_BUTTON_MODE_INIT_PRIORITY);

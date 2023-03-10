#include <hal/nrf_gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "platform/gpio.h"
#include "platform/pwm.h"
#include "platform/shutdown.h"
#include "services/battery.h"
#include "services/debounce.h"

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
    }
    else if (lifecycle_state == normal_operation) {
        // this timer is used to wait for button being held
        // if it is held long enough, shutdown must be performed
        // here we indicate that shutdown is ready, actual shutdown is performed on button release
        pwm_schedule_sequence_from_zero((struct led_value) {80, 0}, SHORT_FADE_MS);
        lifecycle_state = waiting_for_shutdown;
    }
    else if (lifecycle_state == waiting_for_shutdown) {
        shutdown();
    }
}

K_TIMER_DEFINE(lifecycle_timer, lifecycle_timer_cb, NULL);

static void button_mode_gpio_callback(uint32_t pin, bool new_value) {
    if (lifecycle_state == freshly_booted) {
        int timer_running = k_timer_remaining_get(&lifecycle_timer);
        if (new_value && !timer_running) {
            // button released, schedule led fade off if not scheduled yet
            k_timer_start(&lifecycle_timer, K_SECONDS(3), K_FOREVER);
        }
    }
    else if (lifecycle_state == normal_operation) {
        if (!new_value) {
            // button pressed, shutdown will be triggered
            // unless timer will be stopped before timeout by button release
            k_timer_start(&lifecycle_timer, K_MSEC(CONFIG_POWER_ON_OFF_HOLD_TIME), K_FOREVER);
        } else {
            // button released before timeout, stop timer
            k_timer_stop(&lifecycle_timer);
        }
    }
    else if (lifecycle_state == waiting_for_shutdown) {
        // button released after shutdown ready indication, shutdown
        pwm_schedule_sequence_from_last((struct led_value) {0, 0}, SHORT_FADE_MS);
        // wait for led to turn off and debounce to not trigger boot immediately
        k_timer_start(&lifecycle_timer, K_MSEC(SHORT_FADE_MS), K_FOREVER);
    }

    debounce_set_edge_cb(pin, button_mode_gpio_callback);
}

static int button_mode_init(const struct device* dev) {
    ARG_UNUSED(dev);

    // todo: this is a hack, battery service should be used transparently
    battery_trigger_measurement();
    k_usleep(70);

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

    // handle current button state and configure interrupt
    button_mode_gpio_callback(PINOF(button_mode), nrf_gpio_pin_read(PINOF(button_mode)));

    return 0;
}

SYS_INIT(button_mode_init, APPLICATION, CONFIG_APP_BUTTON_MODE_INIT_PRIORITY);

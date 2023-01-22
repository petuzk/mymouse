#include <stdbool.h>
#include <stdint.h>

#include <hal/nrf_gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "platform/gpio.h"
#include "services/debounce.h"
#include "services/hid/source.h"
#include "services/hid/collector.h"
#include "util/toggle_queue.h"

LOG_MODULE_REGISTER(hid_src_buttons);

static const uint32_t button_pins[] = {
    FOR_EACH(PINOF, (,)
        ,button_left
        ,button_right
        ,button_middle
    ),
};

// array of toggle queues per button
// the order of buttons might be diffferent from button_pins (see queue_for_pin)
static struct toggle_queue queues[ARRAY_SIZE(button_pins)];

static inline struct toggle_queue* queue_for_pin(uint32_t pin) {
    // the min/max search is verified to be optimized away in compile time
    uint32_t min = button_pins[0];
    uint32_t max = button_pins[0];
    for (int i = 1; i < ARRAY_SIZE(button_pins); i++) {
        if (button_pins[i] < min) min = button_pins[i];
        if (button_pins[i] > max) max = button_pins[i];
    }
    // check if elements of the array are contiguous (not necessarily sorted)
    // only one branch ends up in the bytecode, the other one is discarded
    if (max - min == ARRAY_SIZE(button_pins) - 1) {
        // calculate the index by substracting min value
        if (min <= pin && pin <= max) {
            return &queues[pin - min];
        }
        return NULL;
    }
    // search with a loop
    for (int i = 0; i < ARRAY_SIZE(button_pins); i++) {
        if (pin == button_pins[i]) {
            return &queues[i];
        }
    }
    return NULL;
}

static void hid_producer_buttons_report_filler(struct hid_report* report);
HID_SOURCE_REGISTER(hid_src_buttons, hid_producer_buttons_report_filler, CONFIG_APP_HID_SOURCE_BUTTONS_PRIORITY);

static void hid_producer_buttons_report_filler(struct hid_report* report) {
    report->buttons.s.left   = toggle_queue_get_or_last(queue_for_pin(PINOF(button_left)));
    report->buttons.s.right  = toggle_queue_get_or_last(queue_for_pin(PINOF(button_right)));
    report->buttons.s.middle = toggle_queue_get_or_last(queue_for_pin(PINOF(button_middle)));
    // if there is still some data, notify
    for (int i = 0; i < ARRAY_SIZE(queues); i++) {
        if (toggle_queue_len(&queues[i])) {
            hid_collector_notify_data_available(hid_src_buttons);
            break;
        }
    }
}

static void hid_src_buttons_interrupt_cb(uint32_t pin, bool new_level) {
    // buttons are active low, hence invert level
    if (unlikely(toggle_queue_put(queue_for_pin(pin), !new_level) < 0)) {
        LOG_WRN("can't put level %d into queue for pin %u", new_level, pin);
        return;
    }
    hid_collector_notify_data_available(hid_src_buttons);
}

static int hid_src_buttons_init(const struct device* dev) {
    ARG_UNUSED(dev);

    for (int i = 0; i < ARRAY_SIZE(button_pins); i++) {
        // buttons are active low, hence invert level
        toggle_queue_init(queue_for_pin(button_pins[i]), !nrf_gpio_pin_read(button_pins[i]));
        debounce_set_edge_cb(button_pins[i], hid_src_buttons_interrupt_cb);
    }

    return 0;
}

SYS_INIT(hid_src_buttons_init, APPLICATION, CONFIG_APP_HID_SOURCE_INIT_PRIORITY);

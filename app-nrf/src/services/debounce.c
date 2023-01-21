/* Button debounce service. Provides interrupt-like interface to "debounced" logic states.
 * Handles hardware interrupt and debounce timings.
 *
 * The implementation is based on a periodically shifted array:
 *
 *       values are shifted by one cell with period T
 *     +---------+    +---------+           +---------+
 *     | value 0 | << | value 1 | << ... << | value N |
 *     +---------+    +---------+           +---------+
 *
 * Once the service receives a hardware GPIO interrupt, it disables the hardware interrupt
 * and puts a re-enable request (by setting bit in a bitmask) into last cell of the array.
 * A timer of period T shifts bitmask requests by one cell and enables hardware interrupts
 * for pins specified by bitmask in cell 0 after shift. This gives the configurable
 * debounce time range [T(N-1), TN].
 *
 * T is configured by APP_DEBOUNCE_TIMER_PERIOD option
 * N is configured by APP_DEBOUNCE_NUM_SHIFTS option
 */

#include "services/debounce.h"

#include <stdint.h>

#include <hal/nrf_gpio.h>
#include <zephyr/kernel.h>

#include "platform/gpio.h"

#define SHIFTARR_SIZE (CONFIG_APP_DEBOUNCE_NUM_SHIFTS + 1)

static uint32_t shiftarr[SHIFTARR_SIZE] = {};
static int shiftarr_cell0_index = 0;
static int shiftarr_moves_left = 0;
static debounce_edge_cb callbacks[32] = {};

static void debounce_timer_cb(struct k_timer *timer);
K_TIMER_DEFINE(debounce_timer, debounce_timer_cb, NULL);

static inline int circular_index_to_phys(int index) {
    index += shiftarr_cell0_index;
    return (index >= SHIFTARR_SIZE) ? index - SHIFTARR_SIZE : index;
}

static inline int rotate_buffer() {
    return shiftarr_cell0_index = circular_index_to_phys(1);
}

static void debounce_gpio_cb(uint32_t pin, bool new_level) {
    // put re-enable request into last cell
    WRITE_BIT(shiftarr[circular_index_to_phys(CONFIG_APP_DEBOUNCE_NUM_SHIFTS)], pin, 1);
    // update the number of elements to be processed
    shiftarr_moves_left = CONFIG_APP_DEBOUNCE_NUM_SHIFTS;
    // start the timer if it's not running already
    if (k_timer_remaining_ticks(&debounce_timer) == 0) {
        k_timer_start(&debounce_timer, K_USEC(CONFIG_APP_DEBOUNCE_TIMER_PERIOD), K_USEC(CONFIG_APP_DEBOUNCE_TIMER_PERIOD));
    }
    // call the user function
    callbacks[pin](pin, new_level);
}

static void debounce_timer_cb(struct k_timer *timer) {
    // rotate the buffer
    int new_head_phys_index = rotate_buffer();
    // re-enable gpio edge interrupt if requested
    if (shiftarr[new_head_phys_index]) {
        gpio_set_edge_cb_mask(shiftarr[new_head_phys_index], nrf_gpio_port_in_read(NRF_P0), debounce_gpio_cb);
        shiftarr[new_head_phys_index] = 0;
    }
    // stop the timer if everything is processed
    if (--shiftarr_moves_left == 0) {
        k_timer_stop(timer);
    }
}

void debounce_set_edge_cb(uint32_t pin, debounce_edge_cb callback) {
    callbacks[pin] = callback;
    gpio_set_edge_cb(pin, nrf_gpio_pin_read(pin), debounce_gpio_cb);
}

void debounce_unset_edge_cb(uint32_t pin) {
    unsigned key = irq_lock();
    gpio_unset_edge_cb(pin);
    for (int i = 1; i <= shiftarr_moves_left; i++) {
        WRITE_BIT(shiftarr[circular_index_to_phys(i)], pin, 0);
    }
    irq_unlock(key);
}

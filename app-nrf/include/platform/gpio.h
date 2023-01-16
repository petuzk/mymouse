#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/devicetree.h>

typedef void (*gpio_edge_cb)(uint32_t pin, bool new_level);

/**
 * @brief Enable one-time GPIO interrupt for the level change on given pins.
 *
 * Because the SENSE mechanism uses common DETECT signal for all pins, the
 * pins are disconnected from this signal after the DETECT condition is met
 * so that other pins could generate rising edge on DETECT signal as well.
 * Therefore it is user's responsibility to re-configure one-time interrupt
 * if needed.
 *
 * @param pinmask    Bitmask of pins to enable interrupt for
 * @param from_level Pin levels to sense level change from (only bits which are set in @c pinmask are considered)
 * @param callback   Callback to be called once the edge is detected (called from ISR context)
 */
void gpio_set_edge_cb_mask(uint32_t pinmask, uint32_t from_level, gpio_edge_cb callback);

static inline void gpio_set_edge_cb(uint32_t pin, bool from_level, gpio_edge_cb callback) {
    gpio_set_edge_cb_mask(1 << pin, from_level << pin, callback);
}

void gpio_unset_edge_cb_mask(uint32_t pinmask);

static inline void gpio_unset_edge_cb(uint32_t pin) {
    gpio_unset_edge_cb_mask(1 << pin);
}

#define PINOFPROP(nodelabel, propname) DT_GPIO_PIN(DT_NODELABEL(nodelabel), propname)
#define PINOF(nodelabel) PINOFPROP(nodelabel, gpios)

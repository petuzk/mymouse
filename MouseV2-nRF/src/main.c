/*
 * Copyright (c) 2021 Taras Radchenko
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <drivers/gpio.h>

#include "bluetooth.h"
#include "hids.h"

// static void user_entry(const struct device *adns7530, void *p2, void *p3)
// {
// 	// hello_world_print(dev);
// }

#if DT_NUM_INST_STATUS_OKAY(nordic_nrf_gpio) != 1
#error "the code expects there is only one GPIO controller"
#endif

#define LED_RED_PIN     DT_GPIO_PIN(DT_NODELABEL(led_red), gpios)
#define LED_RED_FLAGS   DT_GPIO_FLAGS(DT_NODELABEL(led_red), gpios)
#define LED_GREEN_PIN   DT_GPIO_PIN(DT_NODELABEL(led_green), gpios)
#define LED_GREEN_FLAGS DT_GPIO_FLAGS(DT_NODELABEL(led_green), gpios)

#define B_LEFT_PIN      DT_GPIO_PIN(DT_NODELABEL(button_l), gpios)
#define B_LEFT_FLAGS    DT_GPIO_FLAGS(DT_NODELABEL(button_l), gpios)

void main(void) {
	int rv;
	struct sensor_value dx, dy;
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);
	const struct device *adns7530 = DEVICE_DT_GET_ONE(pixart_adns7530);

	rv = gpio_pin_configure(gpio, LED_RED_PIN, GPIO_OUTPUT_INACTIVE | LED_RED_FLAGS);
	if (rv < 0) {
		return;
	}

	rv = gpio_pin_configure(gpio, LED_GREEN_PIN, GPIO_OUTPUT_INACTIVE | LED_GREEN_FLAGS);
	if (rv < 0) {
		return;
	}

	rv = gpio_pin_configure(gpio, B_LEFT_PIN, GPIO_INPUT | B_LEFT_FLAGS);
	if (rv < 0) {
		return;
	}

	if (!device_is_ready(adns7530)) {
		bool led_on = true;
		while (true) {
			gpio_pin_set(gpio, LED_RED_PIN, led_on);
			led_on = !led_on;
			k_msleep(1000);
		}
	}

#ifdef CONFIG_PRJ_FLASH_ON_BOOT
	for (int i = 0; i < 3; i++) {
		gpio_pin_set(gpio, LED_GREEN_PIN, true);
		k_msleep(200);
		gpio_pin_set(gpio, LED_GREEN_PIN, false);
		k_msleep(200);
	}
#endif

	if (mv2_bt_init()) {
		return;
	}

	if (mv2_hids_init()) {
		return;
	}

	while (true) {
		sensor_sample_fetch(adns7530);
		sensor_channel_get(adns7530, SENSOR_CHAN_POS_DX, &dx);
		sensor_channel_get(adns7530, SENSOR_CHAN_POS_DY, &dy);

		bool motion_detected = dx.val1 || dy.val1;
		gpio_pin_set(gpio, LED_GREEN_PIN, motion_detected);

		rv = gpio_pin_get(gpio, B_LEFT_PIN);
		if (rv < 0) {
			return;
		}

		mv2_hids_send_movement(dx.val1, -dy.val1);
		mv2_hids_send_buttons_wheel(rv, false, false, 0);

		k_msleep(1);
	}

	// k_object_access_grant(adns7530, k_current_get());
	// k_thread_user_mode_enter(user_entry, adns7530, NULL, NULL);
}

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

#if DT_NUM_INST_STATUS_OKAY(nordic_nrf_gpio) != 1
#error "the code expects there is only one GPIO controller"
#endif

#define LED_RED_PIN     DT_GPIO_PIN(DT_NODELABEL(led_red), gpios)
#define LED_RED_FLAGS   DT_GPIO_FLAGS(DT_NODELABEL(led_red), gpios)
#define LED_GREEN_PIN   DT_GPIO_PIN(DT_NODELABEL(led_green), gpios)
#define LED_GREEN_FLAGS DT_GPIO_FLAGS(DT_NODELABEL(led_green), gpios)

// Note: sample period for the QDEC was changed to NRF_QDEC_SAMPLEPER_256us (see qdec_nrfx_init)
#define QDEC_A_PIN      DT_PROP(DT_NODELABEL(qdec), a_pin)
#define QDEC_B_PIN      DT_PROP(DT_NODELABEL(qdec), b_pin)
#define QDEC_A_FLAGS    GPIO_PULL_UP  // idk how to add
#define QDEC_B_FLAGS    GPIO_PULL_UP  // this to devicetree

#define BUTTON_L_PIN    DT_GPIO_PIN(DT_NODELABEL(button_l), gpios)
#define BUTTON_L_FLAGS  DT_GPIO_FLAGS(DT_NODELABEL(button_l), gpios)
#define BUTTON_R_PIN    DT_GPIO_PIN(DT_NODELABEL(button_r), gpios)
#define BUTTON_R_FLAGS  DT_GPIO_FLAGS(DT_NODELABEL(button_r), gpios)
#define BUTTON_M_PIN    DT_GPIO_PIN(DT_NODELABEL(button_m), gpios)
#define BUTTON_M_FLAGS  DT_GPIO_FLAGS(DT_NODELABEL(button_m), gpios)

void main(void) {
	// custom accumulator to divide by 2 (a single click makes two transitions) and not loose remainders
	int rot_acc = 0;
	int rv, left, right, middle;
	struct sensor_value dx, dy, rot;
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);
	const struct device *qdec = DEVICE_DT_GET_ONE(nordic_nrf_qdec);
	const struct device *adns7530 = DEVICE_DT_GET_ONE(pixart_adns7530);

	rv = gpio_pin_configure(gpio, LED_RED_PIN, GPIO_OUTPUT_INACTIVE | LED_RED_FLAGS);
	if (rv < 0) {
		return;
	}

	rv = gpio_pin_configure(gpio, LED_GREEN_PIN, GPIO_OUTPUT_INACTIVE | LED_GREEN_FLAGS);
	if (rv < 0) {
		return;
	}

	rv = gpio_pin_configure(gpio, BUTTON_L_PIN, GPIO_INPUT | BUTTON_L_FLAGS);
	if (rv < 0) {
		return;
	}

	rv = gpio_pin_configure(gpio, BUTTON_R_PIN, GPIO_INPUT | BUTTON_R_FLAGS);
	if (rv < 0) {
		return;
	}

	rv = gpio_pin_configure(gpio, BUTTON_M_PIN, GPIO_INPUT | BUTTON_M_FLAGS);
	if (rv < 0) {
		return;
	}

	rv = gpio_pin_configure(gpio, QDEC_A_PIN, QDEC_A_FLAGS);
	if (rv < 0) {
		return;
	}

	rv = gpio_pin_configure(gpio, QDEC_B_PIN, QDEC_B_FLAGS);
	if (rv < 0) {
		return;
	}

	if (!device_is_ready(qdec)) {
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

		sensor_sample_fetch(qdec);
		sensor_channel_get(qdec, SENSOR_CHAN_ROTATION, &rot);
		rot_acc += rot.val1;

		bool motion_detected = dx.val1 || dy.val1;
		gpio_pin_set(gpio, LED_GREEN_PIN, motion_detected);

		left = gpio_pin_get(gpio, BUTTON_L_PIN);
		right = gpio_pin_get(gpio, BUTTON_R_PIN);
		middle = gpio_pin_get(gpio, BUTTON_M_PIN);
		if (left < 0 || right < 0 || middle < 0) {
			return;
		}

		mv2_hids_send_movement(dx.val1, -dy.val1);
		mv2_hids_send_buttons_wheel(left, right, middle, rot_acc / 2);

		rot_acc = rot_acc % 2;
		k_msleep(1);
	}
}

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
#include <pm/pm.h>
#include <hal/nrf_gpio.h>

#include "bluetooth.h"
#include "hids.h"

#if DT_NUM_INST_STATUS_OKAY(nordic_nrf_gpio) != 1
#error "the code expects there is only one GPIO controller"
#endif

#define LED_RED_PIN          DT_GPIO_PIN(DT_NODELABEL(led_red), gpios)
#define LED_RED_FLAGS        DT_GPIO_FLAGS(DT_NODELABEL(led_red), gpios)
#define LED_GREEN_PIN        DT_GPIO_PIN(DT_NODELABEL(led_green), gpios)
#define LED_GREEN_FLAGS      DT_GPIO_FLAGS(DT_NODELABEL(led_green), gpios)

// Note: sample period for the QDEC was changed to NRF_QDEC_SAMPLEPER_256us (see qdec_nrfx_init)
#define QDEC_A_PIN           DT_PROP(DT_NODELABEL(qdec), a_pin)
#define QDEC_B_PIN           DT_PROP(DT_NODELABEL(qdec), b_pin)
#define QDEC_A_FLAGS         GPIO_PULL_UP  // idk how to add
#define QDEC_B_FLAGS         GPIO_PULL_UP  // this to devicetree

#define BUTTON_L_PIN         DT_GPIO_PIN(DT_NODELABEL(button_l), gpios)
#define BUTTON_L_FLAGS       DT_GPIO_FLAGS(DT_NODELABEL(button_l), gpios)
#define BUTTON_R_PIN         DT_GPIO_PIN(DT_NODELABEL(button_r), gpios)
#define BUTTON_R_FLAGS       DT_GPIO_FLAGS(DT_NODELABEL(button_r), gpios)
#define BUTTON_M_PIN         DT_GPIO_PIN(DT_NODELABEL(button_m), gpios)
#define BUTTON_M_FLAGS       DT_GPIO_FLAGS(DT_NODELABEL(button_m), gpios)
#define BUTTON_MODE_PIN      DT_GPIO_PIN(DT_NODELABEL(button_mode), gpios)
#define BUTTON_MODE_FLAGS    DT_GPIO_FLAGS(DT_NODELABEL(button_mode), gpios)

void schedule_poweroff(struct k_timer *timer) {
	// this function should return immediately, but before entering poweroff state
	// we have to wait until the mode button is released, hence poweroff is "scheduled"
	gpio_pin_set(DEVICE_DT_GET_ONE(nordic_nrf_gpio), LED_RED_PIN, true);
	int *poweroff_scheduled = k_timer_user_data_get(timer);
	*poweroff_scheduled = 1;
}

void main(void) {
	// custom accumulator to divide by 2 (a single click makes two transitions) and not loose remainders
	int rot_acc = 0;
	int rv, left, right, middle;
	int prev_btnmode_status, poweroff_scheduled;
	struct k_timer poweroff_timer;
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

	rv = gpio_pin_configure(gpio, BUTTON_MODE_PIN, GPIO_INPUT | BUTTON_MODE_FLAGS);
	if (rv < 0) {
		return;
	}

	const nrf_gpio_pin_sense_t sense = (BUTTON_MODE_FLAGS & GPIO_ACTIVE_LOW) ? NRF_GPIO_PIN_SENSE_LOW : NRF_GPIO_PIN_SENSE_HIGH;
	nrf_gpio_cfg_sense_set(BUTTON_MODE_PIN, sense);

	poweroff_scheduled = 0;
	prev_btnmode_status = gpio_pin_get(gpio, BUTTON_MODE_PIN);
	if (prev_btnmode_status < 0) {
		return;
	}

	k_timer_init(&poweroff_timer, schedule_poweroff, NULL);
	k_timer_user_data_set(&poweroff_timer, &poweroff_scheduled);

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
		rv = gpio_pin_get(gpio, BUTTON_MODE_PIN);
		if (rv < 0) {
			return;
		}
		else if (rv == 0 && poweroff_scheduled) {
			k_msleep(100);  // debounce
			gpio_pin_set(gpio, LED_RED_PIN, false);
			gpio_pin_set(gpio, LED_GREEN_PIN, false);
			pm_power_state_set((struct pm_state_info){PM_STATE_SOFT_OFF});
		}
		else if (rv != prev_btnmode_status) {
			prev_btnmode_status = rv;
			bool timer_running = k_timer_remaining_ticks(&poweroff_timer) != 0;

			if (rv == 1 && !timer_running) {
				k_timer_start(&poweroff_timer, K_MSEC(CONFIG_PRJ_POWEROFF_HOLD_TIME), K_NO_WAIT);
			}
			else if (rv == 0 && timer_running) {
				k_timer_stop(&poweroff_timer);
			}
		}

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

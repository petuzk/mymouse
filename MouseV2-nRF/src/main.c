/*
 * Copyright (c) 2021 Taras Radchenko
 *
 * SPDX-License-Identifier: MIT
 */

#include "sys.h"
#include "bluetooth.h"
#include "battery.h"
#include "hids.h"
#include "fs.h"
#include "lua_worker.h"

static inline int client_update_cycle() {
	// custom accumulator to divide by 2 (a single click makes two transitions) and not loose remainders
	static int rot_acc = 0;
	int left, right, middle;
	struct sensor_value dx, dy, rot;
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);
	const struct device *qdec = DEVICE_DT_GET_ONE(nordic_nrf_qdec);
	const struct device *adns7530 = DEVICE_DT_GET_ONE(pixart_adns7530);

	/* Read */
	sensor_sample_fetch(adns7530);
	sensor_channel_get(adns7530, SENSOR_CHAN_POS_DX, &dx);
	sensor_channel_get(adns7530, SENSOR_CHAN_POS_DY, &dy);

	sensor_sample_fetch(qdec);
	sensor_channel_get(qdec, SENSOR_CHAN_ROTATION, &rot);
	rot_acc += rot.val1;

	left = gpio_pin_get(gpio, BUTTON_L_PIN);
	right = gpio_pin_get(gpio, BUTTON_R_PIN);
	middle = gpio_pin_get(gpio, BUTTON_M_PIN);
	if (left < 0 || right < 0 || middle < 0) {
		return MIN(MIN(left, right), middle);
	}

	/* Write */
	mv2_hids_send_movement(dx.val1, -dy.val1);
	mv2_hids_send_buttons_wheel(left, right, middle, rot_acc / 2);

	rot_acc = rot_acc % 2;
	return 0;
}

static inline int send_events_to_lua() {
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);

	for (uint8_t i = 0; i < MV2_LW_NUM_PROG_BUTTONS; i++) {
		int rv = gpio_pin_get(gpio, prog_btn_pins[i]);
		if (rv < 0) {
			return rv;
		}
		else if (rv != (prog_btn_counters[i] & 1)) {
			prog_btn_counters[i]++;
		}
	}

	return 0;
}

void main(void) {
	int rv, prev_btnmode_status = 1;
	struct mv2_sys_boot_opt boot_opt;
	struct k_timer bat_measurement_timer;
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);

	if (mv2_sys_init(&boot_opt)) return;
	if (mv2_bat_init()) return;
	if (mv2_fs_init()) return;

#ifndef CONFIG_DEBUG
	if (mv2_bt_init(
#ifdef CONFIG_PRJ_BT_DIRECTED_ADVERTISING
		boot_opt.public_adv
#endif // CONFIG_PRJ_BT_DIRECTED_ADVERTISING
	)) return;
#endif // CONFIG_DEBUG

	if (mv2_hids_init()) return;

	k_timer_init(&bat_measurement_timer, NULL, NULL);
	k_timer_start(&bat_measurement_timer, K_NO_WAIT, K_MSEC(CONFIG_PRJ_BAT_MEASUREMENT_PERIOD));

	while (mv2_sys_should_run()) {
		rv = gpio_pin_get(gpio, BUTTON_MODE_PIN);
		if (rv < 0) {
			return;
		}
		else if (rv != prev_btnmode_status) {
			prev_btnmode_status = rv;

			if (rv == 1) {
				mv2_sys_start_poweroff_countdown();
			} else {
				mv2_sys_cancel_poweroff_countdown();
			}
		}

		if (current_client || IS_ENABLED(CONFIG_DEBUG)) {
			if (client_update_cycle() || send_events_to_lua()) {
				return;
			}
		}

		if (k_timer_status_get(&bat_measurement_timer)) {
			millivolts_t mv;
			rv = mv2_bat_sample_mv(&mv);
			if (rv) {
				return;
			}

			bt_bas_set_battery_level((mv2_bat_convert_mv2pts(mv) + 50) / 100);
		}

		k_msleep(1);
	}
}

#include "hids.h"

static bool boot_mode = false;

BT_HIDS_DEF(hids_obj,
	INPUT_REP_BUTTONS_LEN,
	INPUT_REP_MOVEMENT_LEN,
	INPUT_REP_MPLAYER_LEN);

static void mv2_hids_pm_evt_handler(enum bt_hids_pm_evt evt, struct bt_conn *conn) {
	if (!conn || conn != current_client) {
		return;
	}

	switch (evt) {
	case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
		boot_mode = true;
		break;

	case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
		boot_mode = false;
		break;
	}
}

/**
 * @brief Initialize HID service.
 *
 * @return 0 on success, negative error code otherwise.
 */
int mv2_hids_init() {
	struct bt_hids_init_param hids_init_param = { 0 };
	struct bt_hids_inp_rep *hids_inp_rep;
	static const uint8_t mouse_movement_mask[ceiling_fraction(INPUT_REP_MOVEMENT_LEN, 8)] = {0};

	static const uint8_t report_map[] = {
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x02,       /* Usage (Mouse) */

		0xA1, 0x01,       /* Collection (Application) */

		/* Report ID 1: Mouse buttons + scroll/pan */
		0x85, 0x01,       /* Report Id 1 */
		0x09, 0x01,       /* Usage (Pointer) */
		0xA1, 0x00,       /* Collection (Physical) */
		0x95, 0x05,           /* Report Count (5) */
		0x75, 0x01,           /* Report Size (1) */
		0x05, 0x09,               /* Usage Page (Buttons) */
		0x19, 0x01,               /* Usage Minimum (01) */
		0x29, 0x05,               /* Usage Maximum (05) */
		0x15, 0x00,               /* Logical Minimum (0) */
		0x25, 0x01,               /* Logical Maximum (1) */
		0x81, 0x02,               /* Input (Data, Variable, Absolute) */
		0x95, 0x01,           /* Report Count (1) */
		0x75, 0x03,           /* Report Size (3) */
		0x81, 0x01,               /* Input (Constant) for padding */
		0x75, 0x08,           /* Report Size (8) */
		0x95, 0x01,           /* Report Count (1) */
		0x05, 0x01,               /* Usage Page (Generic Desktop) */
		0x09, 0x38,               /* Usage (Wheel) */
		0x15, 0x81,               /* Logical Minimum (-127) */
		0x25, 0x7F,               /* Logical Maximum (127) */
		0x81, 0x06,               /* Input (Data, Variable, Relative) */
		0xC0,             /* End Collection (Physical) */

		/* Report ID 2: Mouse motion */
		0x85, 0x02,       /* Report Id 2 */
		0x09, 0x01,       /* Usage (Pointer) */
		0xA1, 0x00,       /* Collection (Physical) */
		0x75, 0x0C,           /* Report Size (12) */
		0x95, 0x02,           /* Report Count (2) */
		0x05, 0x01,               /* Usage Page (Generic Desktop) */
		0x09, 0x30,               /* Usage (X) */
		0x09, 0x31,               /* Usage (Y) */
		0x16, 0x01, 0xF8,         /* Logical maximum (2047) */
		0x26, 0xFF, 0x07,         /* Logical minimum (-2047) */
		0x81, 0x06,               /* Input (Data, Variable, Relative) */
		0xC0,             /* End Collection (Physical) */
		0xC0,             /* End Collection (Application) */

		/* Report ID 3: Advanced buttons */
		0x05, 0x0C,       /* Usage Page (Consumer) */
		0x09, 0x01,       /* Usage (Consumer Control) */
		0xA1, 0x01,       /* Collection (Application) */
		0x85, 0x03,       /* Report Id (3) */
		0x15, 0x00,       /* Logical minimum (0) */
		0x25, 0x01,       /* Logical maximum (1) */
		0x75, 0x01,       /* Report Size (1) */
		0x95, 0x01,       /* Report Count (1) */

		0x09, 0xCD,       /* Usage (Play/Pause) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x83, 0x01, /* Usage (Consumer Control Configuration) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB5,       /* Usage (Scan Next Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB6,       /* Usage (Scan Previous Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */

		0x09, 0xEA,       /* Usage (Volume Down) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xE9,       /* Usage (Volume Up) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x25, 0x02, /* Usage (AC Forward) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x24, 0x02, /* Usage (AC Back) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0xC0              /* End Collection */
	};

	hids_init_param.rep_map.data = report_map;
	hids_init_param.rep_map.size = sizeof(report_map);

	hids_init_param.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_param.info.b_country_code = 0x00;
	hids_init_param.info.flags = (BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE);

	hids_inp_rep = &hids_init_param.inp_rep_group_init.reports[0];
	hids_inp_rep->size = INPUT_REP_BUTTONS_LEN;
	hids_inp_rep->id = INPUT_REP_REF_BUTTONS_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MOVEMENT_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MOVEMENT_ID;
	hids_inp_rep->rep_mask = mouse_movement_mask;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MPLAYER_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MPLAYER_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_init_param.is_mouse = true;
	hids_init_param.pm_evt_handler = mv2_hids_pm_evt_handler;

	return bt_hids_init(&hids_obj, &hids_init_param);
}

int mv2_hids_send_movement(int16_t delta_x, int16_t delta_y) {
	if (!current_client) {
		return -ENODEV;
	}

	if (boot_mode) {
		delta_x = MAX(MIN(delta_x, SCHAR_MAX), SCHAR_MIN);
		delta_y = MAX(MIN(delta_y, SCHAR_MAX), SCHAR_MIN);

		return bt_hids_boot_mouse_inp_rep_send(
			&hids_obj,
			current_client,
			NULL,
			(int8_t) delta_x,
			(int8_t) delta_y,
			NULL);
	} else {
		uint8_t x_buff[2];
		uint8_t y_buff[2];
		uint8_t buffer[INPUT_REP_MOVEMENT_LEN];

		int16_t x = MAX(MIN(delta_x, 0x07ff), -0x07ff);
		int16_t y = MAX(MIN(delta_y, 0x07ff), -0x07ff);

		/* Convert to little-endian. */
		sys_put_le16(x, x_buff);
		sys_put_le16(y, y_buff);

		/* Encode report. */
		BUILD_ASSERT(sizeof(buffer) == 3, "Only 2 axis, 12-bit each, are supported");

		buffer[0] = x_buff[0];
		buffer[1] = (y_buff[0] << 4) | (x_buff[1] & 0x0f);
		buffer[2] = (y_buff[1] << 4) | (y_buff[0] >> 4);

		return bt_hids_inp_rep_send(
			&hids_obj,
			current_client,
			INPUT_REP_MOVEMENT_IDX,
			buffer,
			sizeof(buffer),
			NULL);
	}
}

int mv2_hids_send_buttons_wheel(bool left, bool right, bool middle, int8_t wheel) {
	uint8_t buffer[INPUT_REP_BUTTONS_LEN];
	union {
		int8_t i;
		uint8_t u;
	} wheel_union = { .i = wheel };

	buffer[0] = (left << 0) | (right << 1) | (middle << 2);
	buffer[1] = wheel_union.u;

	return bt_hids_inp_rep_send(
		&hids_obj,
		current_client,
		INPUT_REP_BUTTONS_IDX,
		buffer,
		sizeof(buffer),
		NULL);
}

int mv2_hids_connected(struct bt_conn *conn) {
	boot_mode = false;
	return bt_hids_connected(&hids_obj, conn);
}

int mv2_hids_disconnected(struct bt_conn *conn) {
	return bt_hids_disconnected(&hids_obj, conn);
}

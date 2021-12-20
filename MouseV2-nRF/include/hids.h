#ifndef MOUSEV2_HIDS_H
#define MOUSEV2_HIDS_H

#include <zephyr.h>
#include <sys/byteorder.h>

#include <bluetooth/conn.h>
#include <bluetooth/services/hids.h>

#include "state.h"

#define BASE_USB_HID_SPEC_VERSION   0x0101

// Number of input reports in this application
#define INPUT_REPORT_COUNT          3
// Length of Mouse Input Report containing button/movement/media player data
#define INPUT_REP_BUTTONS_LEN       3
#define INPUT_REP_MOVEMENT_LEN      3
#define INPUT_REP_MPLAYER_LEN       1
// Index of Mouse Input Report containing button/movement/media player data
#define INPUT_REP_BUTTONS_IDX       0
#define INPUT_REP_MOVEMENT_IDX      1
#define INPUT_REP_MPLAYER_IDX       2
// Id of reference to Mouse Input Report containing button/movement/media player data
#define INPUT_REP_REF_BUTTONS_ID    1
#define INPUT_REP_REF_MOVEMENT_ID   2
#define INPUT_REP_REF_MPLAYER_ID    3

int mv2_hids_init();
int mv2_hids_send_movement(int16_t delta_x, int16_t delta_y);
int mv2_hids_send_buttons_wheel(bool left, bool right, bool middle, int8_t wheel);

int mv2_hids_connected(struct bt_conn *conn);
int mv2_hids_disconnected(struct bt_conn *conn);

#endif // MOUSEV2_HIDS_H

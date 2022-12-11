#pragma once

#include <zephyr/bluetooth/conn.h>

#include "hid_report_struct.h"

int transport_bt_hids_init();
int transport_bt_hids_connected(struct bt_conn *conn);
int transport_bt_hids_disconnected(struct bt_conn *conn);
int transport_bt_hids_deinit();
int transport_bt_send(struct hid_report* report);

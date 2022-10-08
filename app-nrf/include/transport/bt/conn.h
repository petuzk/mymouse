#pragma once

extern struct bt_conn *current_client;

int transport_bt_conn_init();
int transport_bt_available();

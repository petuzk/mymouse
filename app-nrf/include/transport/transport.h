#pragma once

#include "hid_report_struct.h"

// typedef int (*tranport_init_cb)();
// typedef int (*tranport_deinit_cb)();
typedef int (*tranport_available)();
typedef int (*tranport_send_cb)(struct hid_report*);
typedef int (*tranport_upd_bat_lvl_cb)(int);

struct transport {
    // tranport_init_cb   init;
    // tranport_deinit_cb deinit;
    tranport_available      available;
    tranport_send_cb        send;
    tranport_upd_bat_lvl_cb upd_bat_lvl;
};

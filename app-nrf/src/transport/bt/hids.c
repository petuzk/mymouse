#include "transport/bt/hids.h"

#include "transport/bt/conn.h"

#include <bluetooth/services/hids.h>

#include <zephyr/logging/log.h>

#include "hid_report_map.h"
#include "hid_report_struct.h"

LOG_MODULE_DECLARE(transport_bt);

#define BASE_USB_HID_SPEC_VERSION 0x0101

BT_HIDS_DEF(hids_obj, sizeof(struct hid_report));

void transport_bt_hids_pm_evt_handler(enum bt_hids_pm_evt evt, struct bt_conn *conn) {
    if (!conn || conn != current_client) {
        LOG_WRN("got event %d from conn %p", evt, (void*) conn);
        return;
    }

    switch (evt) {
        case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
            LOG_INF("entered boot mode");
            break;

        case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
            LOG_INF("entered report mode");
            break;

        default:
            LOG_INF("got event %d", evt);
            break;
    }
}

static struct bt_hids_init_param hids_init_param = {
    .info = {
        .bcd_hid = BASE_USB_HID_SPEC_VERSION,
        .b_country_code = 0x00,
        .flags = (BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE),
    },
    .rep_map = {
        .data = hid_report_map,
        .size = HID_REPORT_MAP_SIZE,
    },
    .inp_rep_group_init = {
        .reports = {
            {
                .id = HID_REPORT_ID,
                .size = sizeof(struct hid_report),
                .rep_mask = &hid_report_persistent_bytes,
            },
        },
        .cnt = 1,
    },
    // boot mode is unsupported yet
    .is_mouse = true,
    .pm_evt_handler = transport_bt_hids_pm_evt_handler,
};

int transport_bt_hids_init() {
    return bt_hids_init(&hids_obj, &hids_init_param);
}

int transport_bt_hids_connected(struct bt_conn *conn) {
    return bt_hids_connected(&hids_obj, conn);
}

int transport_bt_hids_disconnected(struct bt_conn *conn) {
    return bt_hids_disconnected(&hids_obj, conn);
}

// int transport_bt_hids_deinit() {
//     return bt_hids_uninit(&hids_obj);
// }

int transport_bt_send(struct hid_report* report) {
    return bt_hids_inp_rep_send(
        &hids_obj,
        current_client,
        0, // currently there's only one report defined
        (uint8_t*) report,
        sizeof(struct hid_report),
        NULL);
}

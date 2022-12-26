#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "hid_report_struct.h"
#include "transport/bt/transport.h"

LOG_MODULE_REGISTER(hid_sink_transport);

void hid_sink_impl(struct hid_report* report) {
    if (!bt_transport.available()) {
        // todo: block collector when the sink (transport)
        // is unavailable and reset sources when it becomes available
        return;
    }
    int err = bt_transport.send(report);
    if (err) {
        LOG_WRN("send returned %d", err);
    }
    k_sleep(K_MSEC(10));
}

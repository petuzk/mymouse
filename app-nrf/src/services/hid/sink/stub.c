#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "hid_report_struct.h"

LOG_MODULE_REGISTER(hid_sink_stub);

void hid_sink_impl(struct hid_report* report) {
    LOG_INF("buttons=%02x, movement=(%03x, %03x), wheel=%02x",
        report->buttons.v, report->x_delta, report->y_delta, report->wheel_delta);
    // long delay for easy manual testing with a shell
    k_sleep(K_SECONDS(5));
}

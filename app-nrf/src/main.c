#include <logging/log.h>
#include <zephyr.h>

#include "hidenc.h"
#include "hid_report_struct.h"
#include "transport/bt/transport.h"
#include "services/button_mode.h"

LOG_MODULE_REGISTER(main);

void main() {
    struct hid_report report = {};
    bool updated = false;

    while (true) {
        check_button_mode();

        if (bt_transport.available()) {
            updated = hidenc_maybe_update_client(&bt_transport, &report);
        }

        k_msleep(updated ? 10 : 2);
    }
}

#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include "platform/qdec.h"
#include "services/hid/collector.h"
#include "services/hid/source.h"

static int delta = 0;

static void hid_src_encoder_report_filler(struct hid_report* report) {
    unsigned key = irq_lock();
    report->wheel_delta = delta;
    delta = 0;
    irq_unlock(key);
}

HID_SOURCE_REGISTER(hid_src_encoder, hid_src_encoder_report_filler, CONFIG_APP_HID_SOURCE_ENCODER_PRIORITY);

static void hid_src_encoder_qdec_data_callback(int value) {
    // every second click should increment the delta
    static int acc = 0;
    acc += value;
    delta = acc / 2;
    acc = acc % 2;
    if (delta) {
        hid_collector_notify_data_available(hid_src_encoder);
    }
}

static int hid_src_encoder_init(const struct device* dev) {
    ARG_UNUSED(dev);

    qdec_set_data_cb(hid_src_encoder_qdec_data_callback);

    return 0;
}

SYS_INIT(hid_src_encoder_init, APPLICATION, CONFIG_APP_HID_SOURCE_INIT_PRIORITY);

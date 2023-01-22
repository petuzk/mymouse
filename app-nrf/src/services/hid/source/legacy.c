#include <stdbool.h>

#include <hal/nrf_gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "devices.h"
#include "hid_report_struct.h"
#include "platform/gpio.h"
#include "platform/qdec.h"
#include "shell/report.h"
#include "services/hid/collector.h"
#include "services/hid/source.h"
#include "transport/transport.h"

LOG_MODULE_REGISTER(hidenc);

static inline void fill_rotation(struct hid_report *report, bool *updated) {
    static int acc = 0;

#ifdef CONFIG_APP_SHELL_HID_REPORT
    if (report_shell_fill_rotation(report)) {
        *updated = true;
        return;
    }
#endif

    int value = qdec_read_acc();

    if (value) {
        acc += value;
        int delta = acc / 2;
        if (delta) {
            *updated = true;
            report->wheel_delta = delta;
            acc = acc % 2;
        }
    }
}

static inline void fill_movement(struct hid_report *report, bool *updated) {
    struct sensor_value value;

#ifdef CONFIG_APP_SHELL_HID_REPORT
    if (report_shell_fill_movement(report)) {
        *updated = true;
        return;
    }
#endif

    if (nrf_gpio_pin_read(PINOFPROP(optical_sensor, mot_gpios)) == 0) {
        // motion interrupt low -> data available, read will clear it
        *updated = true;

        sensor_sample_fetch(optical_sensor);
        sensor_channel_get(optical_sensor, SENSOR_CHAN_POS_DX, &value);
        report->x_delta = value.val1;
        sensor_channel_get(optical_sensor, SENSOR_CHAN_POS_DY, &value);
        report->y_delta = -value.val1;
    }
}

static void legacy_polling_report_filler(struct hid_report* report);
HID_SOURCE_REGISTER(hid_src_legacy, legacy_polling_report_filler, CONFIG_APP_HID_SOURCE_LEGACY_PRIORITY);

static void legacy_polling_report_filler(struct hid_report* report) {
    bool updated = false;
    fill_rotation(report, &updated);
    fill_movement(report, &updated);

    if (!updated) {
        k_sleep(K_MSEC(2));
    }

    // ask the collector to get data from us again
    hid_collector_notify_data_available(hid_src_legacy);
}

// todo: hidenc should probably be split & moved to services
// until then, initialize sensor's interrupt gpio here

static int hidenc_init(const struct device* dev) {
    ARG_UNUSED(dev);

    nrf_gpio_cfg_input(PINOFPROP(optical_sensor, mot_gpios), NRF_GPIO_PIN_NOPULL);
    hid_collector_notify_data_available(hid_src_legacy);

    return 0;
}

SYS_INIT(hidenc_init, APPLICATION, CONFIG_APP_HID_SOURCE_INIT_PRIORITY);

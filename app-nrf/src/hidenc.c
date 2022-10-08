#include "hidenc.h"

#include <stdbool.h>

#include <drivers/sensor.h>
#include <hal/nrf_gpio.h>
#include <logging/log.h>
#include <zephyr.h>

#include "devices.h"
#include "hid_report_struct.h"
#include "platform/gpio.h"
#include "platform/qdec.h"
#include "shell/report.h"
#include "transport/transport.h"

LOG_MODULE_REGISTER(hidenc);

static inline void fill_buttons(struct hid_report *report, bool *updated) {
    static const uint32_t btn_port_mask = \
        BIT(PINOF(button_left)) | BIT(PINOF(button_right)) | BIT(PINOF(button_middle));
    static uint32_t prev_port_state = 0;
    uint32_t port_state = ~(nrf_gpio_port_in_read(NRF_GPIO) & btn_port_mask);

    if (port_state != prev_port_state) {
        *updated = true;
        prev_port_state = port_state;
        report->buttons.s.left   = (port_state >> PINOF(button_left)) & 1;
        report->buttons.s.right  = (port_state >> PINOF(button_right)) & 1;
        report->buttons.s.middle = (port_state >> PINOF(button_middle)) & 1;
    }
}

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

static inline void clear_non_persistent_data_in_report(struct hid_report *report) {
    report->wheel_delta = 0;
    report->x_delta = 0;
    report->y_delta = 0;
}

bool hidenc_maybe_update_client(struct transport* transport, struct hid_report* report) {
    int err = 0;
    bool updated = false;

    fill_buttons(report, &updated);
    fill_rotation(report, &updated);
    fill_movement(report, &updated);

    if (updated) {
        if ((err = transport->send(report))) {
            LOG_WRN("send returned %d", err);
        }
        clear_non_persistent_data_in_report(report);
    }

    return updated;
}
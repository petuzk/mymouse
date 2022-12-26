#pragma once

#include <stdint.h>

#include "hid_report_map.h"

struct __attribute__((__packed__)) hid_report {
    union {
        struct {
            uint8_t left   : 1;
            uint8_t right  : 1;
            uint8_t middle : 1;
            uint8_t unused : 5;
        } s;
        uint8_t v;
    } buttons;

    uint8_t wheel_delta;

    uint16_t x_delta : 12;
    uint16_t y_delta : 12;
};

static inline void clear_non_persistent_data_in_report(struct hid_report *report) {
    uint8_t* report_bytes = (uint8_t*) report;
    // compiler is smart enough to unwrap this loop!
    for (int i = 0; i < sizeof(struct hid_report); i++) {
        if (!((hid_report_persistent_bytes >> i) & 1)) {
            report_bytes[i] = 0;
        }
    }
}

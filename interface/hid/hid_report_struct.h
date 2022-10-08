#pragma once

#include <stdint.h>

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

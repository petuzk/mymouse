#pragma once

#include "hid_report_struct.h"

typedef void (*report_filler_t)(struct hid_report*);

struct hid_source {
    const char* name;
    report_filler_t report_filler;
};

#pragma once

#include "hid_report_struct.h"

// called from collector, must be implemented by a sink
void hid_sink_impl(struct hid_report* report);

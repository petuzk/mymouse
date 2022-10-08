#pragma once

#ifdef CONFIG_APP_SHELL_HID_REPORT

#include <stdbool.h>

#include "hid_report_struct.h"

bool report_shell_fill_movement(struct hid_report *report);
bool report_shell_fill_rotation(struct hid_report *report);

#endif // CONFIG_APP_SHELL_HID_REPORT

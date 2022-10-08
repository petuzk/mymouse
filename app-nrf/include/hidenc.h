#pragma once

#include <stdbool.h>

#include "hid_report_struct.h"
#include "transport/transport.h"

bool hidenc_maybe_update_client(struct transport* transport, struct hid_report* report);

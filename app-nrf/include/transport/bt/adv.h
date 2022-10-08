#pragma once

#include <stdbool.h>

void transport_bt_adv_init(bool public_adv);
void transport_bt_adv_next();
bool transport_bt_adv_was_public_adv_requested();

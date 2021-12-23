#ifndef MOUSEV2_BLUETOOTH_H
#define MOUSEV2_BLUETOOTH_H

/* According to HID over GATT profile specification:
 *   The HID Device shall have one or more instances of the HID Service,
 *   one or more instances of the Battery Service,
 *   one instance of the Device Information Service [...]
 */

#include <zephyr.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/services/bas.h>
#include <bluetooth/services/hrs.h>
#include <bluetooth/services/nus.h>

#include "state.h"
#include "hids.h"
#include "mouseconnect.h"

#if CONFIG_PRJ_BT_DIRECTED_ADVERTISING
#define PUBLIC_ADV_PARAM  bool public_adv
#define PUBLIC_ADV_ARG    public_adv
#define PUBLIC_ADV_TRUE   true
#define PUBLIC_ADV_FALSE  false
#else
#define PUBLIC_ADV_PARAM
#define PUBLIC_ADV_ARG
#define PUBLIC_ADV_TRUE
#define PUBLIC_ADV_FALSE
#endif

int mv2_bt_init(PUBLIC_ADV_PARAM);

#endif // MOUSEV2_BLUETOOTH_H

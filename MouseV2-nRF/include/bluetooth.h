#ifndef MOUSEV2_BLUETOOTH_H
#define MOUSEV2_BLUETOOTH_H

/* According to HID over GATT profile specification:
 *   The HID Device shall have one or more instances of the HID Service,
 *   one or more instances of the Battery Service,
 *   one instance of the Device Information Service [...]
 */

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/services/bas.h>
#include <bluetooth/services/hrs.h>

int mv2_bt_init();

#endif // MOUSEV2_BLUETOOTH_H

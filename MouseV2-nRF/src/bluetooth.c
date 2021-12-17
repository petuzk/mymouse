#include "bluetooth.h"

static const struct bt_data adv_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		BT_UUID_16_ENCODE(BT_UUID_BAS_VAL))
};

int mv2_bt_init() {
	int rv = bt_enable(NULL);
	if (rv) {
		return rv;
	}

	settings_load();

	rv = bt_le_adv_start(BT_LE_ADV_CONN_NAME, adv_data, ARRAY_SIZE(adv_data), NULL, 0);
	if (rv) {
		return rv;
	}

	return 0;
}

#include "bluetooth.h"

static struct k_work adv_work;
static const struct bt_data adv_data[] = {
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		(CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		(CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),  // dont include '\0'
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

#if CONFIG_PRJ_BT_DIRECTED_ADVERTISING
/**
 * @brief Direct advertising addresses queue (DAAQ).
 *
 * @see mv2_bt_advertising_process
 */
K_MSGQ_DEFINE(bonds_queue, sizeof(bt_addr_le_t), CONFIG_BT_MAX_PAIRED, 4);
#endif

/**
 * @brief Starts advertising process.
 *
 * If directed advertising is enabled and DAAQ is not empty, the address is poped
 * from the queue and directed advertising starts. Otherwise, undirected advertising starts.
 *
 * @param work Unused
 */
static void mv2_bt_advertising_process(struct k_work *work) {
	ARG_UNUSED(work);

	struct bt_le_adv_param adv_param;

#if CONFIG_PRJ_BT_DIRECTED_ADVERTISING
	bt_addr_le_t addr;

	if (!k_msgq_get(&bonds_queue, &addr, K_NO_WAIT)) {
		adv_param = *BT_LE_ADV_CONN_DIR(&addr);
		adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;

		bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);
	} else
#endif
	{
		adv_param = *BT_LE_ADV_CONN;
		adv_param.options |= BT_LE_ADV_OPT_ONE_TIME;
		bt_le_adv_start(&adv_param, adv_data, ARRAY_SIZE(adv_data), NULL, 0);
	}
}

#if CONFIG_PRJ_BT_DIRECTED_ADVERTISING
/**
 * @brief Puts bond (bound device) information to the DAAQ.
 *
 * @param info Bond info
 * @param user_data User-supplied data (unused)
 */
static void mv2_bt_put_bond_to_queue(const struct bt_bond_info *info, void *user_data) {
	ARG_UNUSED(user_data);

	// Do not put address of the already connected client
	// Only makes sense when multiple devices can be connected simultaneously,
	// because otherwise advertising will not be started when client is connected.
	// if (current_client) {
	// 	if (!bt_addr_le_cmp(&info->addr, bt_conn_get_dst(current_client))) {
	// 		return;
	// 	}
	// }

	k_msgq_put(&bonds_queue, (void *) &info->addr, K_NO_WAIT);
}
#endif

/**
 * @brief Start advertising.
 *
 * If direct advertising is enabled, this function firstly initializes DAAQ with known bonds.
 */
static void mv2_bt_advertising_start() {
#if CONFIG_PRJ_BT_DIRECTED_ADVERTISING
	k_msgq_purge(&bonds_queue);
	bt_foreach_bond(BT_ID_DEFAULT, mv2_bt_put_bond_to_queue, NULL);
#endif

	k_work_submit(&adv_work);
}

/**
 * @brief Connection (error) handler.
 *
 * On connection error, restarts advertising.
 * If connection succeeded, saves the client and notifies HID service.
 *
 * @param conn New connection object
 * @param err HCI error. Zero for success, non-zero otherwise
 */
static void mv2_bt_connected_callback(struct bt_conn *conn, uint8_t err) {
	if (err) {
		if (err == BT_HCI_ERR_ADV_TIMEOUT) {
			// advertise to the next device from DAAQ or start undirected advertising
			k_work_submit(&adv_work);
		}
		return;
	}

	if (!(current_client = bt_conn_ref(conn))) {
		return;
	}

	mv2_hids_connected(conn);
}

/**
 * @brief Disconnection handler.
 *
 * Notifies HID service and restarts advertising.
 *
 * @param conn Connection object.
 * @param reason HCI reason for the disconnection.
 */
static void mv2_bt_disconnected_callback(struct bt_conn *conn, uint8_t reason) {
	ARG_UNUSED(reason);

	mv2_hids_disconnected(conn);

	bt_conn_unref(current_client);
	current_client = NULL;

	mv2_bt_advertising_start();
}

/**
 * @brief Connection callbacks
 */
static struct bt_conn_cb conn_callbacks = {
	.connected = mv2_bt_connected_callback,
	.disconnected = mv2_bt_disconnected_callback,
};

// static void mv2_bt_pairing_confirm(struct bt_conn *conn) {
// 	bt_conn_auth_pairing_confirm(conn);
// }

// static struct bt_conn_auth_cb conn_auth_callbacks = {
// 	.pairing_confirm = mv2_bt_pairing_confirm,
// };

/**
 * @brief Initialize Bluetooth subsystem and start advertising.
 *
 * @return 0 on success, negative error code otherwise.
 */
int mv2_bt_init() {
	bt_conn_cb_register(&conn_callbacks);

#ifdef CONFIG_BT_FIXED_PASSKEY
	// conn_auth_cb is not required in this case,
	// providing fixed passkey implies DisplayOnly capability
	bt_passkey_set(CONFIG_PRJ_BT_FIXED_PASSKEY_VALUE);
#endif
	// ... or is it needed to confirm pairing?
	// bt_conn_auth_cb_register(&conn_auth_callbacks);

	int rv = bt_enable(NULL);
	if (rv) {
		return rv;
	}

	settings_load();

	// k_work_init(&hids_work, mouse_handler);
	// k_work_init(&pairing_work, pairing_process);
	k_work_init(&adv_work, mv2_bt_advertising_process);

	mv2_bt_advertising_start();

	return 0;
}

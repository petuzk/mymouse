#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <bluetooth/services/nus.h>

LOG_MODULE_DECLARE(transport_bt);

static bool public_adv_requested = false;

bool transport_bt_adv_was_public_adv_requested() {
    return public_adv_requested;
}

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

static const struct bt_data scan_data[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

/**
 * @brief Direct advertising addresses queue (DAAQ).
 */
K_MSGQ_DEFINE(bonds_queue, sizeof(bt_addr_le_t), CONFIG_BT_MAX_PAIRED, 4);

/**
 * @brief Puts bond (bound device) address to the DAAQ.
 */
static void put_bond_to_queue(const struct bt_bond_info *info, void *user_data) {
    ARG_UNUSED(user_data);

    // Do not put address of the already connected client
    // Only makes sense when multiple devices can be connected simultaneously,
    // because otherwise advertising will not be started when client is connected.
    // if (current_client) {
    //     if (!bt_addr_le_cmp(&info->addr, bt_conn_get_dst(current_client))) {
    //         return;
    //     }
    // }

    k_msgq_put(&bonds_queue, &info->addr, K_NO_WAIT);
}

void transport_bt_adv_init(bool public_adv) {
    public_adv_requested = public_adv;
    k_msgq_purge(&bonds_queue);
    if (public_adv) {
        LOG_INF("init undir adv");
        k_msgq_put(&bonds_queue, BT_ADDR_LE_ANY, K_NO_WAIT);
    } else {
        LOG_INF("init dir adv");
        bt_foreach_bond(BT_ID_DEFAULT, put_bond_to_queue, NULL);
    }
}

/**
 * @brief Runs advertising.
 *
 * If directed advertising is enabled and DAAQ is not empty, the address is poped
 * from the queue and directed advertising starts. Otherwise, undirected advertising starts.
 *
 * @param work Unused
 */
static void advertising_run(struct k_work *work) {
    ARG_UNUSED(work);

    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_t addr;
    struct bt_le_adv_param adv_param;

    if (k_msgq_get(&bonds_queue, &addr, K_NO_WAIT)) {
        // the queue is empty, nothing to do
        return;
    }

    if (bt_addr_le_cmp(&addr, BT_ADDR_LE_ANY)) {
        // not a BT_ADDR_LE_ANY, start directed advertising
        bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
        LOG_INF("run dir adv to %s", addr_str);
        adv_param = *BT_LE_ADV_CONN_DIR(&addr);
        bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);
    } else {
        LOG_INF("run undir adv");
        adv_param = *BT_LE_ADV_CONN;
        adv_param.options |= BT_LE_ADV_OPT_ONE_TIME;
        bt_le_adv_start(&adv_param, adv_data, ARRAY_SIZE(adv_data), scan_data, ARRAY_SIZE(scan_data));
    }
}

K_WORK_DEFINE(adv_work, advertising_run);

void transport_bt_adv_next() {
    k_work_submit(&adv_work);
}

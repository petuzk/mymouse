#include "transport/bt/conn.h"

#include <bluetooth/hci.h>
#include <host/hci_core.h>
#include <logging/log.h>

#include "transport/bt/adv.h"
#include "transport/bt/hids.h"

LOG_MODULE_DECLARE(transport_bt);

struct bt_conn *current_client = NULL;
bt_security_t security_level = BT_SECURITY_L1;

int transport_bt_available() {
    return current_client != NULL && (
        security_level == BT_SECURITY_L2 ||
        security_level == BT_SECURITY_L3 ||
        security_level == BT_SECURITY_L4);
}

static void bt_connected_callback(struct bt_conn *conn, uint8_t err) {
    if (err) {
        if (err == BT_HCI_ERR_ADV_TIMEOUT) {
            LOG_INF("advertising timeout");
            transport_bt_adv_next();
        } else {
            LOG_WRN("connection failed with err %d", err);
        }
        return;
    }

    if (!(current_client = bt_conn_ref(conn))) {
        return;
    }

    transport_bt_hids_connected(conn);
}

static void bt_disconnected_callback(struct bt_conn *conn, uint8_t reason) {
    if (conn != current_client) {
        // does it mean advertising should be restarted?
        LOG_WRN("unknown client disconnected, reason %d", reason);
        return;
    }

    transport_bt_hids_disconnected(conn);

    // if public advertising was previously requested, but current connection
    // did not reach to meet "transport available" condition, restart public advertising
    transport_bt_adv_init(transport_bt_adv_was_public_adv_requested() && !transport_bt_available());
    transport_bt_adv_next();

    bt_conn_unref(current_client);
    current_client = NULL;
    security_level = BT_SECURITY_L1;
}

static void bt_identity_resolved_callback(struct bt_conn *conn, const bt_addr_le_t *rpa, const bt_addr_le_t *identity) {
    char rpa_str[BT_ADDR_LE_STR_LEN], identity_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(rpa, rpa_str, sizeof(rpa_str));
    bt_addr_le_to_str(identity, identity_str, sizeof(identity_str));
    LOG_INF("identity resolved: rpa %s, identity %s", log_strdup(rpa_str), log_strdup(identity_str));
}

static void bt_security_changed_callback(struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {
    if (conn != current_client) {
        LOG_WRN("security changed for unknown connection: level %d, err %d", level, err);
        return;
    }

    security_level = level;
    LOG_INF("security changed: level %d, err %d", level, err);
}

static struct bt_conn_cb conn_callbacks = {
    .connected = bt_connected_callback,
    .disconnected = bt_disconnected_callback,
    .identity_resolved = bt_identity_resolved_callback,
    .security_changed = bt_security_changed_callback,
};

int transport_bt_conn_init() {
    bt_conn_cb_register(&conn_callbacks);
    return 0;
}

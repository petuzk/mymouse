#include "transport/bt/transport.h"

#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "services/bootopts.h"
#include "transport/bt/adv.h"
#include "transport/bt/conn.h"
#include "transport/bt/hids.h"
#include "transport/transport.h"

LOG_MODULE_REGISTER(transport_bt);

static int transport_bt_init(const struct device *dev) {
    ARG_UNUSED(dev);
    LOG_INF("initializing");

    int rv;

    transport_bt_conn_init();

    rv = bt_enable(NULL);
    if (rv) {
        return rv;
    }

    rv = settings_load();
    if (rv) {
        return rv;
    }

    transport_bt_hids_init();

    transport_bt_adv_init(boot_options.bt_public_adv);
    transport_bt_adv_next();

    return 0;
}

int transport_bt_upd_bat_lvl(int level) {
    return bt_bas_set_battery_level(level);
}

// int transport_bt_deinit() {
//     return transport_bt_hids_deinit();
// }

struct transport bt_transport = {
    // .init      = transport_bt_init,
    // .deinit    = transport_bt_deinit,
    .available   = transport_bt_available,
    .send        = transport_bt_send,
    .upd_bat_lvl = transport_bt_upd_bat_lvl,
};

SYS_INIT(transport_bt_init, APPLICATION, CONFIG_APP_TRANSPORT_INIT_PRIORITY);

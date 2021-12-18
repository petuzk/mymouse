#ifndef MV2_STATE_H
#define MV2_STATE_H

#include <bluetooth/conn.h>

/**
 * @brief Currently connected Bluetooth client or NULL if there is no connected device
 */
extern struct bt_conn *current_client;

#endif // MV2_STATE_H

#pragma once

#include <stdint.h>

uint8_t prepare_response(uint8_t command_id, uint8_t* num_tx, const uint8_t** tx_data, uint8_t* num_rx, uint8_t** rx_data);
void process_transaction(uint8_t command_id, uint8_t num_tx, const uint8_t* tx_data, uint8_t num_rx, const uint8_t* rx_data);

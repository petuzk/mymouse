#include "spi_commands.h"

static const uint8_t device_id_response[] = {0x1E, 0x93, 0x89};

uint8_t prepare_response(
    const uint8_t command_id,
    uint8_t* num_tx, const uint8_t** tx_data,
    uint8_t* num_rx, uint8_t** rx_data
) {
    uint8_t first_tx_byte = 0;
    switch (command_id) {
        case 0x06:
            first_tx_byte = *device_id_response;
            *tx_data = device_id_response + 1;
            *num_tx = sizeof(device_id_response) - 1;
            break;
    }
    return first_tx_byte;
}

void process_transaction(
    const uint8_t command_id,
    const uint8_t num_tx, const uint8_t* tx_data,
    const uint8_t num_rx, const uint8_t* rx_data
) {
    // TBD
}

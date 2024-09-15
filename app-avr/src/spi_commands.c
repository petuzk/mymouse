#include "spi_commands.h"

#include <LUFA/Drivers/USB/USB.h>

#include "descriptors.h"

static const uint8_t device_id_response[] = {0x1E, 0x93, 0x89};
static uint8_t rx_buf[8];
static uint8_t report_sizes[MAX_REPORTS_NUM + 1];

uint8_t prepare_response(
    const uint8_t command_id,
    uint8_t* num_tx, const uint8_t** tx_data,
    uint8_t* num_rx, uint8_t** rx_data
) {
    uint8_t first_tx_byte = 0;
    if (command_id == 0x06) { // read device ID
        first_tx_byte = *device_id_response;
        *tx_data = device_id_response + 1;
        *num_tx = sizeof(device_id_response) - 1;
    }
    else if (command_id == 0x07) { // set report size
        *rx_data = report_sizes;
        *num_rx = sizeof(report_sizes);
    }
    else if (command_id == 0x08) { // set report descriptor
        get_hid_report_descriptor_buffer(num_rx, rx_data);
    }
    else if (command_id == 0x09) { // enable USB controller
        *rx_data = rx_buf;
        *num_rx = 1;
    }
    return first_tx_byte;
}

void process_transaction(
    const uint8_t command_id,
    const uint8_t num_tx, const uint8_t* tx_data,
    const uint8_t num_rx, const uint8_t* rx_data
) {
    if (command_id == 0x07) { // set report size
        set_hid_report_size(report_sizes[MAX_REPORTS_NUM]);
    }
    else if (command_id == 0x09) { // enable USB
        if (rx_data[0]) USB_Init();
    }
}

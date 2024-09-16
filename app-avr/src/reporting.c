#include "reporting.h"

#include <stddef.h>

#include <LUFA/Drivers/USB/USB.h>

#include "descriptors.h"

void send_report(const uint8_t size, const uint8_t* data) {
    if (USB_DeviceState != DEVICE_STATE_Configured) {
        return;
    }
    Endpoint_SelectEndpoint(MOUSE_EPADDR);

    if (Endpoint_IsReadWriteAllowed()) {
        Endpoint_Write_Stream_LE(data, size, NULL);
        Endpoint_ClearIN();
    }
}

/*
             LUFA Library
     Copyright (C) Dean Camera, 2021.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2021  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  USB Device Descriptors, for library use when in USB device mode. Descriptors are special
 *  computer-readable structures which the host requests upon device enumeration, to determine
 *  the device's capabilities and functions.
 */

#include <app_bootloader_interface.h>

#include "Descriptors.h"

/** Device descriptor structure. This descriptor, located in FLASH memory, describes the overall
 *  device characteristics, including the supported USB version, control endpoint size and the
 *  number of device configurations. The descriptor is read out by the USB host when the enumeration
 *  process begins.
 */
const USB_Descriptor_Device_t PROGMEM device_descriptor =
{
    .Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

    .USBSpecification       = VERSION_BCD(1,1,0),
    .Class                  = USB_CSCP_NoDeviceClass,
    .SubClass               = USB_CSCP_NoDeviceSubclass,
    .Protocol               = USB_CSCP_NoDeviceProtocol,

    .Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

    .VendorID               = DEVICE_ID_VENDOR,
    .ProductID              = DEVICE_ID_PRODUCT_APP,
    .ReleaseNumber          = VERSION_BCD(0,0,1),

    .ManufacturerStrIndex   = STRING_ID_Manufacturer,
    .ProductStrIndex        = STRING_ID_Product,
    .SerialNumStrIndex      = NO_DESCRIPTOR,

    .NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
};

/** Configuration descriptor structure. This descriptor, located in FLASH memory, describes the usage
 *  of the device in one of its supported configurations, including information about any device interfaces
 *  and endpoints. The descriptor is read out by the USB host during the enumeration process when selecting
 *  a configuration so that the host may correctly communicate with the USB device.
 */
static USB_Descriptor_Configuration_t configuration_descriptor =
{
    .Config =
        {
            .Header                 = {.Size = sizeof(USB_Descriptor_Configuration_Header_t), .Type = DTYPE_Configuration},

            .TotalConfigurationSize = sizeof(USB_Descriptor_Configuration_t),
            .TotalInterfaces        = 1,

            .ConfigurationNumber    = 1,
            .ConfigurationStrIndex  = NO_DESCRIPTOR,
            .ConfigAttributes       = (USB_CONFIG_ATTR_RESERVED | USB_CONFIG_ATTR_SELFPOWERED),
            .MaxPowerConsumption    = USB_CONFIG_POWER_MA(CURRENT_CONSUMPTION_MA)
        },

    .HID_Interface =
        {
            .Header                 = {.Size = sizeof(USB_Descriptor_Interface_t), .Type = DTYPE_Interface},

            .InterfaceNumber        = INTERFACE_ID_Mouse,
            .AlternateSetting       = 0x00,

            .TotalEndpoints         = 1,

            .Class                  = HID_CSCP_HIDClass,
            .SubClass               = HID_CSCP_BootSubclass,
            .Protocol               = HID_CSCP_MouseBootProtocol,

            .InterfaceStrIndex      = NO_DESCRIPTOR
        },

    .HID_MouseHID =
        {
            .Header                 = {.Size = sizeof(USB_HID_Descriptor_HID_t), .Type = HID_DTYPE_HID},

            .HIDSpec                = VERSION_BCD(1,1,1),
            .CountryCode            = 0x00,
            .TotalReportDescriptors = 1,
            .HIDReportType          = HID_DTYPE_Report,
            .HIDReportLength        = 0  // obtained from SPI master device
        },

    .HID_ReportINEndpoint =
        {
            .Header                 = {.Size = sizeof(USB_Descriptor_Endpoint_t), .Type = DTYPE_Endpoint},

            .EndpointAddress        = MOUSE_EPADDR,
            .Attributes             = (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
            .EndpointSize           = MOUSE_EPSIZE,
            .PollingIntervalMS      = 0x05
        }
};

/**
 * @brief Size of a RAM buffer for HID Report descriptor.
 */
#define REPORT_DESCRIPTOR_MAX_SIZE 256

// The full Report descriptor consists of two parts: the Device Control report and the Application report.
// The Device Control report is used to put the AVR into bootloader mode, and it uses report ID 8.
// The Application report is obtained from SPI master, and may define reports 0-7.
// Since the Device Control report has known size, it is put first, and the Application report follows it in memory.
#define DEVICE_CONTROL_REPORT_DESCRIPTOR { \
    /* Vendor control report */ \
    HID_RI_USAGE_PAGE(16, HID_VENDOR_PAGE), /* vendor page 0xDC */ \
    /* usage seem to not be required, it's anyway meaningless for a vendor-defined report */ \
    HID_RI_COLLECTION(8, 0x01), /* application collection */ \
        HID_RI_REPORT_ID(8, HID_REPORTID_DEVICE_CONTROL), \
        HID_RI_LOGICAL_MINIMUM(8, 0x00), \
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF), \
        HID_RI_REPORT_SIZE(8, 0x08), /* 8 bits */ \
        HID_RI_REPORT_COUNT(16, 1), \
        HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE), \
    HID_RI_END_COLLECTION(0), \
    HID_RI_REPORT_ID(8, 0), /* restore current report ID to 0 */ \
}

#define DEVICE_CONTROL_REPORT_SIZE sizeof((uint8_t[])DEVICE_CONTROL_REPORT_DESCRIPTOR)

/**
 * @brief HID Report descriptor.
 *
 * HID Report descriptor is not hardcoded, but instead is obtained from SPI master.
 * Therefore it is stored in RAM.
 */
static uint8_t hid_report_desc[REPORT_DESCRIPTOR_MAX_SIZE] = DEVICE_CONTROL_REPORT_DESCRIPTOR;

void set_hid_report_size(uint8_t size) {
    uint16_t full_size = DEVICE_CONTROL_REPORT_SIZE + (uint16_t)size;
    if (USB_DeviceState == DEVICE_STATE_Unattached && full_size <= REPORT_DESCRIPTOR_MAX_SIZE) {
        configuration_descriptor.HID_MouseHID.HIDReportLength = full_size;
    }
}

void get_hid_report_descriptor_buffer(uint8_t* size, uint8_t** data) {
    if (USB_DeviceState == DEVICE_STATE_Unattached) {
        *size = configuration_descriptor.HID_MouseHID.HIDReportLength - DEVICE_CONTROL_REPORT_SIZE;
        *data = hid_report_desc + DEVICE_CONTROL_REPORT_SIZE;
    } else {
        *size = 0;
    }
}

/** Language descriptor structure. This descriptor, located in FLASH memory, is returned when the host requests
 *  the string descriptor with index 0 (the first index). It is actually an array of 16-bit integers, which indicate
 *  via the language ID table available at USB.org what languages the device supports for its string descriptors.
 */
const USB_Descriptor_String_t PROGMEM language_string = USB_STRING_DESCRIPTOR_ARRAY(LANGUAGE_ID_ENG);

/** Manufacturer descriptor string. This is a Unicode string containing the manufacturer's details in human readable
 *  form, and is read out upon request by the host when the appropriate string ID is requested, listed in the Device
 *  Descriptor.
 */
const USB_Descriptor_String_t PROGMEM manufacturer_string = USB_STRING_DESCRIPTOR(L"LUFA Library");

/** Product descriptor string. This is a Unicode string containing the product's details in human readable form,
 *  and is read out upon request by the host when the appropriate string ID is requested, listed in the Device
 *  Descriptor.
 */
const USB_Descriptor_String_t PROGMEM product_string = USB_STRING_DESCRIPTOR(L"LUFA Mouse Demo");

/** This function is called by the library when in device mode, and must be overridden (see library "USB Descriptors"
 *  documentation) by the application code so that the address and size of a requested descriptor can be given
 *  to the USB library. When the device receives a Get Descriptor request on the control endpoint, this function
 *  is called so that the descriptor details can be passed back and the appropriate descriptor sent back to the
 *  USB host.
 */
uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue,
                                    const uint16_t wIndex,
                                    const void** const descr_address,
                                    uint8_t* memory_space)
{
    const uint8_t descr_type = (wValue >> 8);
    const uint8_t descr_number = (wValue & 0xFF);

    const void* addr = NULL;
    uint16_t size = NO_DESCRIPTOR;
    uint8_t space = MEMSPACE_FLASH;

    switch (descr_type) {
        case DTYPE_Device:
            addr = &device_descriptor;
            size = sizeof(USB_Descriptor_Device_t);
            break;
        case DTYPE_Configuration:
            space = MEMSPACE_RAM;
            addr = &configuration_descriptor;
            size = sizeof(USB_Descriptor_Configuration_t);
            break;
        case DTYPE_String:
            switch (descr_number) {
                case STRING_ID_Language:
                    addr = &language_string;
                    size = pgm_read_byte(&language_string.Header.Size);
                    break;
                case STRING_ID_Manufacturer:
                    addr = &manufacturer_string;
                    size = pgm_read_byte(&manufacturer_string.Header.Size);
                    break;
                case STRING_ID_Product:
                    addr = &product_string;
                    size = pgm_read_byte(&product_string.Header.Size);
                    break;
            }
            break;
        case HID_DTYPE_HID:
            addr = &configuration_descriptor.HID_MouseHID;
            size = sizeof(USB_HID_Descriptor_HID_t);
            break;
        case HID_DTYPE_Report:
            space = MEMSPACE_RAM;
            addr = hid_report_desc;
            size = configuration_descriptor.HID_MouseHID.HIDReportLength;
            break;
    }

    *descr_address = addr;
    *memory_space = space;
    return size;
}

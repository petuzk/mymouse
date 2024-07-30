// This code is based on LUFA BootloaderHID example, distributed under modified MIT license:
// https://github.com/abcminiuser/lufa/blob/a13ca118d3987890b749a2a96a92f74771821f8a/Bootloaders/HID/Descriptors.c

// Copyright 2021 Dean Camera (dean [at] fourwalledcubicle [dot] com)
// Copyright 2024 Taras Radchenko (petuzk.dev@gmail.com)

/**
 * @file Descriptors.c
 * @brief USB Descriptors for the bootloader.
 */

#include <app_bootloader_interface.h>

#include "Descriptors.h"

/** Bootloader only has/needs one USB interface. */
#define INTERFACE_ID 0

/** HID class report descriptor. Only contains the vendor-defined report used to program the device. */
const USB_Descriptor_HIDReport_Datatype_t HIDReport[] =
{
    HID_RI_USAGE_PAGE(16, HID_VENDOR_PAGE),
    // usage seem to not be required, it's anyway meaningless for a vendor-defined report
    HID_RI_COLLECTION(8, 0x01), // application collection
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_REPORT_COUNT(16, (sizeof(uint16_t) + SPM_PAGESIZE)),
        HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE),
    HID_RI_END_COLLECTION(0),
};

/**
 * @brief Device descriptor with basic information about the device.
 *
 * Python script recognizes the device in bootloader mode by Vendor and Product IDs specified here.
 */
const USB_Descriptor_Device_t DeviceDescriptor =
{
    .Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

    .USBSpecification       = VERSION_BCD(1,1,0),
    .Class                  = USB_CSCP_NoDeviceClass,
    .SubClass               = USB_CSCP_NoDeviceSubclass,
    .Protocol               = USB_CSCP_NoDeviceProtocol,

    .Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

    .VendorID               = DEVICE_ID_VENDOR,
    .ProductID              = DEVICE_ID_PRODUCT_BOOTLOADER,
    .ReleaseNumber          = VERSION_BCD(0,0,1),

    .ManufacturerStrIndex   = NO_DESCRIPTOR,
    .ProductStrIndex        = NO_DESCRIPTOR,
    .SerialNumStrIndex      = NO_DESCRIPTOR,

    .NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
};

/** Configuration descriptor structure for this application (see ConfigurationDescriptor below). */
typedef struct {
    USB_Descriptor_Configuration_Header_t Config;

    // generic HID interface
    USB_Descriptor_Interface_t            HID_Interface;
    USB_HID_Descriptor_HID_t              HID_VendorHID;
    USB_Descriptor_Endpoint_t             HID_ReportINEndpoint;
} USB_Descriptor_Configuration_t;

/** Minimal required USB Configuration descriptor for this application. */
const USB_Descriptor_Configuration_t ConfigurationDescriptor =
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

            .InterfaceNumber        = INTERFACE_ID,
            .AlternateSetting       = 0x00,

            .TotalEndpoints         = 1,

            .Class                  = HID_CSCP_HIDClass,
            .SubClass               = HID_CSCP_NonBootSubclass,
            .Protocol               = HID_CSCP_NonBootProtocol,

            .InterfaceStrIndex      = NO_DESCRIPTOR
        },

    .HID_VendorHID =
        {
            .Header                 = {.Size = sizeof(USB_HID_Descriptor_HID_t), .Type = HID_DTYPE_HID},

            .HIDSpec                = VERSION_BCD(1,1,1),
            .CountryCode            = 0x00,
            .TotalReportDescriptors = 1,
            .HIDReportType          = HID_DTYPE_Report,
            .HIDReportLength        = sizeof(HIDReport)
        },

    .HID_ReportINEndpoint =
        {
            .Header                 = {.Size = sizeof(USB_Descriptor_Endpoint_t), .Type = DTYPE_Endpoint},

            .EndpointAddress        = HID_IN_EPADDR,
            .Attributes             = (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
            .EndpointSize           = HID_IN_EPSIZE,
            .PollingIntervalMS      = 0x05
        },
};

/** Called by LUFA to get different kinds of descriptors for this device/application. */
uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue,
                                    const uint16_t wIndex,
                                    const void** const descriptor_address)
{
    const uint8_t DescriptorType   = (wValue >> 8);

    const void* address = NULL;
    uint16_t    size    = NO_DESCRIPTOR;

    if (DescriptorType == DTYPE_Device) {
        address = &DeviceDescriptor;
        size = sizeof(USB_Descriptor_Device_t);
    }
    else if (DescriptorType == DTYPE_Configuration) {
        address = &ConfigurationDescriptor;
        size = sizeof(USB_Descriptor_Configuration_t);
    }
    else if (DescriptorType == HID_DTYPE_HID) {
        address = &ConfigurationDescriptor.HID_VendorHID;
        size = sizeof(USB_HID_Descriptor_HID_t);
    }
    else if (DescriptorType == HID_DTYPE_Report) {
        address = &HIDReport;
        size = sizeof(HIDReport);
    }

    *descriptor_address = address;
    return size;
}

// This code is based on LUFA Mouse example, distributed under modified MIT license:
// https://github.com/abcminiuser/lufa/blob/a13ca118d3987890b749a2a96a92f74771821f8a/Demos/Device/LowLevel/Mouse/Descriptors.h

// Copyright 2021 Dean Camera (dean [at] fourwalledcubicle [dot] com)
// Copyright 2024 Taras Radchenko (petuzk.dev@gmail.com)

#pragma once

#include <stdint.h>

    /* Includes: */
        #include <LUFA/Drivers/USB/USB.h>

        #include <avr/pgmspace.h>

    /* Macros: */
        /** Endpoint address of the Mouse HID reporting IN endpoint. */
        #define MOUSE_EPADDR              (ENDPOINT_DIR_IN | 1)

        /** Size in bytes of the Mouse HID reporting IN endpoint. */
        #define MOUSE_EPSIZE              8

        /**
         * @brief HID Report ID for device status & control.
         *
         * This report is written to by the host to enter bootloader.
         */
        #define HID_REPORTID_DEVICE_CONTROL MAX_REPORTS_NUM

    /* Type Defines: */
        /** Type define for the device configuration descriptor structure. This must be defined in the
         *  application code, as the configuration descriptor contains several sub-descriptors which
         *  vary between devices, and which describe the device's usage to the host.
         */
        typedef struct
        {
            USB_Descriptor_Configuration_Header_t Config;

            // Mouse HID Interface
            USB_Descriptor_Interface_t            HID_Interface;
            USB_HID_Descriptor_HID_t              HID_MouseHID;
            USB_Descriptor_Endpoint_t             HID_ReportINEndpoint;
        } USB_Descriptor_Configuration_t;

        /** Enum for the device interface descriptor IDs within the device. Each interface descriptor
         *  should have a unique ID index associated with it, which can be used to refer to the
         *  interface from other descriptors.
         */
        enum InterfaceDescriptors_t
        {
            INTERFACE_ID_Mouse = 0, /**< Mouse interface descriptor ID */
        };

        /** Enum for the device string descriptor IDs within the device. Each string descriptor should
         *  have a unique ID index associated with it, which can be used to refer to the string from
         *  other descriptors.
         */
        enum StringDescriptors_t
        {
            STRING_ID_Language     = 0, /**< Supported Languages string descriptor ID (must be zero) */
            STRING_ID_Manufacturer = 1, /**< Manufacturer string ID */
            STRING_ID_Product      = 2, /**< Product string ID */
        };

void set_hid_report_size(uint8_t size);
void get_hid_report_descriptor_buffer(uint8_t* size, uint8_t** data);

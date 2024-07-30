#pragma once

/**
 * @brief Vendor ID of the device.
 *
 * Both app and bootloader should report this value.
 */
#define DEVICE_ID_VENDOR 0x03EB

/**
 * @brief Product ID of the device in app mode (i.e. actual mouse).
 */
#define DEVICE_ID_PRODUCT_APP 0x2041

/**
 * @brief Product ID of the device in bootloader mode.
 */
#define DEVICE_ID_PRODUCT_BOOTLOADER 0x2067

/**
 * @brief Vendor-defined usage page number for custom device functions.
 *
 * Vendor-defined usage pages are FF00-FFFF.
 */
#define HID_VENDOR_PAGE 0xFFDC

/**
 * @brief Current consumption in mA reported by USB configuration descriptor.
 *
 * Battery charging current is set to ~370mA, standard allows up to 500mA.
 */
#define CURRENT_CONSUMPTION_MA 444

#include "hid_report_map.h"

const uint8_t hid_report_map[] = {
    /* Report ID 1: Mouse buttons + scroll + movement */
    0x05, 0x01,           /* Usage Page (Generic Desktop) */
    0x09, 0x02,           /* Usage (Mouse) */
    0xA1, 0x01,           /* Collection (Application) */
    0x85, HID_REPORT_ID,  /* Report ID 1 */
    0x09, 0x01,           /* Usage (Pointer) */
    0xA1, 0x00,           /* Collection (Physical) */
    0x95, 0x03,               /* Report Count (3) */
    0x75, 0x01,               /* Report Size (1) */
    0x05, 0x09,                   /* Usage Page (Buttons) */
    0x19, 0x01,                   /* Usage Minimum (01) */
    0x29, 0x03,                   /* Usage Maximum (03) */
    0x15, 0x00,                   /* Logical Minimum (0) */
    0x25, 0x01,                   /* Logical Maximum (1) */
    0x81, 0x02,                   /* Input (Data, Variable, Absolute) */
    0x95, 0x01,               /* Report Count (1) */
    0x75, 0x05,               /* Report Size (5) */
    0x81, 0x01,                   /* Input (Constant) for padding */
    0x95, 0x01,               /* Report Count (1) */
    0x75, 0x08,               /* Report Size (8) */
    0x05, 0x01,                   /* Usage Page (Generic Desktop) */
    0x09, 0x38,                   /* Usage (Wheel) */
    0x15, 0x81,                   /* Logical Minimum (-127) */
    0x25, 0x7F,                   /* Logical Maximum (127) */
    0x81, 0x06,                   /* Input (Data, Variable, Relative) */
    0x95, 0x02,               /* Report Count (2) */
    0x75, 0x0C,               /* Report Size (12) */
    0x05, 0x01,                   /* Usage Page (Generic Desktop) */
    0x09, 0x30,                   /* Usage (X) */
    0x09, 0x31,                   /* Usage (Y) */
    0x16, 0x01, 0xF8,             /* Logical maximum (2047) */
    0x26, 0xFF, 0x07,             /* Logical minimum (-2047) */
    0x81, 0x06,                   /* Input (Data, Variable, Relative) */
    0xC0,                 /* End Collection (Physical) */
    0xC0,                 /* End Collection (Application) */

#if 0
    /* Report ID 2: Advanced buttons */
    0x05, 0x0C,       /* Usage Page (Consumer) */
    0x09, 0x01,       /* Usage (Consumer Control) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, 0x02,       /* Report ID (2) */

    0x15, 0x00,       /* Logical minimum (0) */
    0x25, 0x01,       /* Logical maximum (1) */
    0x75, 0x01,       /* Report Size (1) */
    0x95, 0x01,       /* Report Count (1) */

    0x09, 0xCD,       /* Usage (Play/Pause) */
    0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
    0x0A, 0x83, 0x01, /* Usage (Consumer Control Configuration) */
    0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
    0x09, 0xB5,       /* Usage (Scan Next Track) */
    0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
    0x09, 0xB6,       /* Usage (Scan Previous Track) */
    0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */

    0x09, 0xEA,       /* Usage (Volume Down) */
    0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
    0x09, 0xE9,       /* Usage (Volume Up) */
    0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
    0x0A, 0x25, 0x02, /* Usage (AC Forward) */
    0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
    0x0A, 0x24, 0x02, /* Usage (AC Back) */
    0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
    0xC0              /* End Collection */
#endif
};

_Static_assert(sizeof(hid_report_map) == HID_REPORT_MAP_SIZE);

/**
 * Only the first byte containing button states is persistent,
 * other bytes contain delta values and cannot be read multiple times
 * (in BT transport they are sent once via notification)
 */
const uint8_t hid_report_persistent_bytes = 0x01;

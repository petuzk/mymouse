// This code is based on LUFA BootloaderHID example, distributed under modified MIT license:
// https://github.com/abcminiuser/lufa/blob/a13ca118d3987890b749a2a96a92f74771821f8a/Bootloaders/HID/Descriptors.h

// Copyright 2021 Dean Camera (dean [at] fourwalledcubicle [dot] com)
// Copyright 2024 Taras Radchenko (petuzk.dev@gmail.com)

#pragma once

#include <LUFA/Drivers/USB/USB.h>

/** Endpoint address of the HID data IN endpoint. */
#define HID_IN_EPADDR  (ENDPOINT_DIR_IN | 1)

/** Size in bytes of the HID reporting IN endpoint. */
#define HID_IN_EPSIZE  64

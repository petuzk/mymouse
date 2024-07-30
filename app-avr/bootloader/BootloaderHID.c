// This code is based on LUFA BootloaderHID example, distributed under modified MIT license:
// https://github.com/abcminiuser/lufa/blob/a13ca118d3987890b749a2a96a92f74771821f8a/Bootloaders/HID/BootloaderHID.c

// Copyright 2021 Dean Camera (dean [at] fourwalledcubicle [dot] com)
// Copyright 2024 Taras Radchenko (petuzk.dev@gmail.com)

/**
 * @file BootloaderHID.c
 * @brief Bootloader for flashing application image via USB HID report.
 */

#include <stdbool.h>

#include <avr/wdt.h>
#include <avr/boot.h>
#include <util/atomic.h>
#include <util/delay.h>

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

#include "Descriptors.h"

/** Bootloader special address to start the user application */
#define COMMAND_STARTAPPLICATION   0xFFFF

/** Magic value written to magic_boot_key to jump to application after watchdog reset. */
#define MAGIC_BOOT_KEY             0xDC42

/**
 * @brief Flag indicating if the bootloader should be running.
 *
 * When cleared, the bootloader will shut down the USB interface and wait for a watchdog reset.
 */
static bool run_bootloader = true;

/**
 * @brief Magic value indicating whether the bootloader should start an application after a watchdog reset.
 *
 * This variable is placed in a special section that is not zeroed on start.
 * The bootloader sets this to MAGIC_BOOT_KEY to indicate that flashing is completed, and the application
 * should be started after watchdog reset.
 */
uint16_t magic_boot_key ATTR_NO_INIT;

/**
 * @brief Function performing jump to application code.
 *
 * See https://www.avrfreaks.net/s/topic/a5C3l000000UQYqEAO/t115894?comment=P-941576
 */
extern __attribute__((noreturn)) void jump_to_application() asm ( "0x0 ");

/**
 * @brief Special routine to determine if bootloader or application should be executed.
 *
 * The device switches between bootloader and application via watchdog reset (which also resets the peripherals).
 * If the application wants to enter the bootloader, it should trigger a watchdog reset without setting magic_boot_key
 * (it doesn't know its address anyway). If the bootloader has completed application flashing and wants to boot it,
 * it triggers watchdog reset and writes magic_boot_key. Therefore magic_boot_key determines whether to run application
 * or bootloader when the watchdog reset occurs. For every other reset reason (e.g. power on) skip the bootloader.
 */
void application_jump_check() ATTR_INIT_SECTION(3);

void application_jump_check() {
    // jump to app if reset reason is other than watchdog or if magic_boot_key is correct
    if (!(MCUSR & (1 << WDRF)) || (magic_boot_key == MAGIC_BOOT_KEY))
    {
        // clear the boot key so that it's not correct in case of a subsequent watchdog reset
        magic_boot_key = 0;
        // enable watchdog so that if there is a bug in application making it unresponsive we switch to bootloader
        wdt_enable(WDTO_60MS);
        jump_to_application();
    }

    // reset MCUSR to clear WDRF
    MCUSR = 0;
}

int main() {
    // disable watchdog that may have been started by application
    wdt_disable();

    // relocate the interrupt vector table to the bootloader section
    MCUCR = (1 << IVCE);
    MCUCR = (1 << IVSEL);

    // initialize USB subsystem, and enable global interrupts for LUFA library to function
    USB_Init();
    GlobalInterruptEnable();

    while (run_bootloader) {
        // this is the body of USB_DeviceTask without switching between endpoints,
        // since we don't use any endpoints other than 0 (control endpoint)
        if (USB_DeviceState == DEVICE_STATE_Unattached) {
            continue;
        }

        Endpoint_SelectEndpoint(ENDPOINT_CONTROLEP);

        if (Endpoint_IsSETUPReceived()) {
            USB_Device_ProcessControlRequest();
        }
    }

    // wait a short time to end all USB transactions and then disconnect from USB bus
    _delay_us(1000);
    USB_Detach();

    // set magic_boot_key to start the application after reset, and perform the reset
    magic_boot_key = MAGIC_BOOT_KEY;
    wdt_enable(WDTO_60MS);
    for (;;);
}

/**
 * @brief Event handler called by LUFA to configure device endpoints on USB SetConfiguration request.
 */
void EVENT_USB_Device_ConfigurationChanged() {
    // setup HID report endpoint, it has to be configured even though it's not used
    // all incoming requests will be NAKed
    Endpoint_ConfigureEndpoint(HID_IN_EPADDR, EP_TYPE_INTERRUPT, HID_IN_EPSIZE, 1);
}

/**
 * @brief Event handler called by LUFA to respond to application-specific USB Control request.
 *
 * The bootloader should respond to HID SetReport requests used to program flash memory.
 */
void EVENT_USB_Device_ControlRequest() {
    // ignore any requests that aren't directed to the HID interface
    if ((USB_ControlRequest.bmRequestType & (CONTROL_REQTYPE_TYPE | CONTROL_REQTYPE_RECIPIENT)) !=
        (REQTYPE_CLASS | REQREC_INTERFACE))
    {
        return;
    }

    switch (USB_ControlRequest.bRequest)
    {
        case HID_REQ_SetReport:
            Endpoint_ClearSETUP();

            // wait until the command has been sent by the host
            while (!(Endpoint_IsOUTReceived()));

            // read in the write destination address, and determine if it's aligned to page size
            uint16_t page_address = Endpoint_Read_16_LE();
            bool is_page_address_aligned = !(page_address & (SPM_PAGESIZE - 1));

            // handle "start application" command recognized by the special address
            if (page_address == COMMAND_STARTAPPLICATION)
            {
                run_bootloader = false;
            }
            // reject otherwise invalid addresses
            else if (!(page_address < BOOT_START_ADDR && is_page_address_aligned)) {
                Endpoint_StallTransaction();
            }
            else
            {
                // erase the given flash page
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
                {
                    boot_page_erase(page_address);
                    boot_spm_busy_wait();
                }

                // write flash page byte by byte
                for (uint8_t page_word = 0; page_word < (SPM_PAGESIZE / 2); page_word++)
                {
                    if (!Endpoint_BytesInEndpoint())
                    {
                        Endpoint_ClearOUT();
                        while (!Endpoint_IsOUTReceived());
                    }

                    // write the next data word to flash
                    boot_page_fill(page_address + ((uint16_t)page_word << 1), Endpoint_Read_16_LE());
                }

                // write the filled flash page to memory
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
                {
                    boot_page_write(page_address);
                    boot_spm_busy_wait();
                }

                // re-enable RWW section
                boot_rww_enable();
            }

            // finish transaction
            Endpoint_ClearOUT();
            Endpoint_ClearStatusStage();
            break;
    }
}

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
 *  Main source file for the HID class bootloader. This file contains the complete bootloader logic.
 */

#include "BootloaderHID.h"

/** Flag to indicate if the bootloader should be running, or should exit and allow the application code to run
 *  via a soft reset. When cleared, the bootloader will abort, the USB interface will shut down and the application
 *  started via a forced watchdog reset.
 */
static bool RunBootloader = true;

/** Magic lock for forced application start. If the HWBE fuse is programmed and BOOTRST is unprogrammed, the bootloader
 *  will start if the /HWB line of the AVR is held low and the system is reset. However, if the /HWB line is still held
 *  low when the application attempts to start via a watchdog reset, the bootloader will re-start. If set to the value
 *  \ref MAGIC_BOOT_KEY the special init function \ref Application_Jump_Check() will force the application to start.
 */
uint16_t MagicBootKey ATTR_NO_INIT;

/**
 * @brief Function performing jump to application code.
 *
 * See https://www.avrfreaks.net/s/topic/a5C3l000000UQYqEAO/t115894?comment=P-941576
 */
extern __attribute__((noreturn)) void jump_to_application(void) asm ( "0x0 ");

/**
 * @brief Special routine to determine if bootloader or application should be executed.
 *
 * The device switches between bootloader and application via watchdog reset (which also resets the peripherals).
 * If the application wants to enter the bootloader, it should trigger a watchdog reset without setting MagicBootKey
 * (it doesn't know its address anyway). If the bootloader has completed application flashing and wants to boot it,
 * it triggers watchdog reset and writes MagicBootKey. Therefore MagicBootKey determines whether to run application
 * or bootloader when the watchdog reset occurs. For every other reset reason (e.g. power on) skip the bootloader.
 */
void Application_Jump_Check(void)
{
    // jump to app if reset reason is other than watchdog or if MagicBootKey is correct
    if (!(MCUSR & (1 << WDRF)) || (MagicBootKey == MAGIC_BOOT_KEY))
    {
        // clear the boot key so that it's not correct in case of a subsequent watchdog reset
        MagicBootKey = 0;
        jump_to_application();
    }

    // reset MCUSR to clear WDRF
    MCUSR = 0;
}

/** Main program entry point. This routine configures the hardware required by the bootloader, then continuously
 *  runs the bootloader processing routine until instructed to soft-exit.
 */
int main(void)
{
    /* Setup hardware required for the bootloader */
    SetupHardware();

    /* Enable global interrupts so that the USB stack can function */
    GlobalInterruptEnable();

    while (RunBootloader) {
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

    /* Wait a short time to end all USB transactions and then disconnect */
    _delay_us(1000);

    /* Disconnect from the host - USB interface will be reset later along with the AVR */
    USB_Detach();

    /* Unlock the forced application start mode of the bootloader if it is restarted */
    MagicBootKey = MAGIC_BOOT_KEY;

    /* Enable the watchdog and force a timeout to reset the AVR */
    wdt_enable(WDTO_60MS);

    for (;;);
}

/** Configures all hardware required for the bootloader. */
static void SetupHardware(void)
{
    // disable watchdog that may have been started by application
    wdt_disable();

    /* Relocate the interrupt vector table to the bootloader section */
    MCUCR = (1 << IVCE);
    MCUCR = (1 << IVSEL);

    /* Initialize USB subsystem */
    USB_Init();
}

/** Event handler for the USB_ConfigurationChanged event. This configures the device's endpoints ready
 *  to relay data to and from the attached USB host.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
    /* Setup HID Report Endpoint */
    Endpoint_ConfigureEndpoint(HID_IN_EPADDR, EP_TYPE_INTERRUPT, HID_IN_EPSIZE, 1);
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
    /* Ignore any requests that aren't directed to the HID interface */
    if ((USB_ControlRequest.bmRequestType & (CONTROL_REQTYPE_TYPE | CONTROL_REQTYPE_RECIPIENT)) !=
        (REQTYPE_CLASS | REQREC_INTERFACE))
    {
        return;
    }

    /* Process HID specific control requests */
    switch (USB_ControlRequest.bRequest)
    {
        case HID_REQ_SetReport:
            Endpoint_ClearSETUP();

            /* Wait until the command has been sent by the host */
            while (!(Endpoint_IsOUTReceived()));

            /* Read in the write destination address */
            #if (FLASHEND > 0xFFFF)
            uint32_t PageAddress = ((uint32_t)Endpoint_Read_16_LE() << 8);
            #else
            uint16_t PageAddress = Endpoint_Read_16_LE();
            #endif

            /* Determine if the given page address is correctly aligned to the
               start of a flash page. */
            bool PageAddressIsAligned = !(PageAddress & (SPM_PAGESIZE - 1));

            /* Check if the command is a program page command, or a start application command */
            #if (FLASHEND > 0xFFFF)
            if ((uint16_t)(PageAddress >> 8) == COMMAND_STARTAPPLICATION)
            #else
            if (PageAddress == COMMAND_STARTAPPLICATION)
            #endif
            {
                RunBootloader = false;
            }
            else if ((PageAddress < BOOT_START_ADDR) && PageAddressIsAligned)
            {
                /* Erase the given FLASH page, ready to be programmed */
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
                {
                    boot_page_erase(PageAddress);
                    boot_spm_busy_wait();
                }

                /* Write each of the FLASH page's bytes in sequence */
                for (uint8_t PageWord = 0; PageWord < (SPM_PAGESIZE / 2); PageWord++)
                {
                    /* Check if endpoint is empty - if so clear it and wait until ready for next packet */
                    if (!(Endpoint_BytesInEndpoint()))
                    {
                        Endpoint_ClearOUT();
                        while (!(Endpoint_IsOUTReceived()));
                    }

                    /* Write the next data word to the FLASH page */
                    boot_page_fill(PageAddress + ((uint16_t)PageWord << 1), Endpoint_Read_16_LE());
                }

                /* Write the filled FLASH page to memory */
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
                {
                    boot_page_write(PageAddress);
                    boot_spm_busy_wait();
                }

                /* Re-enable RWW section */
                boot_rww_enable();
            }

            Endpoint_ClearOUT();

            Endpoint_ClearStatusStage();
            break;
    }
}

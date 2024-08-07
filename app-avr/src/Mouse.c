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
 *  Main source file for the Mouse demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#include "Mouse.h"

/** Indicates what report mode the host has requested, true for normal HID reporting mode, \c false for special boot
 *  protocol reporting mode.
 */
static bool UsingReportProtocol = true;

/** Current Idle period. This is set by the host via a Set Idle HID class request to silence the device's reports
 *  for either the entire idle duration, or until the report status changes (e.g. the user moves the mouse).
 */
static uint16_t IdleCount = 0;

/** Current Idle period remaining. When the IdleCount value is set, this tracks the remaining number of idle
 *  milliseconds. This is separate to the IdleCount timer and is incremented and compared as the host may request
 *  the current idle period via a Get Idle HID class request, thus its value must be preserved.
 */
static uint16_t IdleMSRemaining = 0;


/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
int main(void)
{
    SetupHardware();

    GlobalInterruptEnable();

    for (;;)
    {
        wdt_reset();
        Mouse_Task();
        USB_USBTask();
    }
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
    // clear reset reason so that if we need bootloader it will only contain watchdog flag
    MCUSR = 0;

    /* Hardware Initialization */
    USB_Init();
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Device_Connect(void)
{
    /* Default to report protocol on connect */
    UsingReportProtocol = true;
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the USB management and Mouse reporting tasks.
 */
void EVENT_USB_Device_Disconnect(void) {}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host sets the current configuration
 *  of the USB device after enumeration - the device endpoints are configured and the mouse reporting task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool ConfigSuccess = true;

    /* Setup HID Report Endpoint */
    ConfigSuccess &= Endpoint_ConfigureEndpoint(MOUSE_EPADDR, EP_TYPE_INTERRUPT, MOUSE_EPSIZE, 1);

    /* Turn on Start-of-Frame events for tracking HID report period expiry */
    USB_Device_EnableSOFEvents();
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
    /* Handle HID Class specific requests */
    switch (USB_ControlRequest.bRequest)
    {
        case HID_REQ_GetReport:
            if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                USB_MouseReport_Data_t MouseReportData;

                /* Create the next mouse report for transmission to the host */
                CreateMouseReport(&MouseReportData);

                Endpoint_ClearSETUP();

                /* Write the report data to the control endpoint */
                Endpoint_Write_Control_Stream_LE(&MouseReportData, sizeof(MouseReportData));
                Endpoint_ClearOUT();
            }

            break;
        case HID_REQ_SetReport:
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE) &&
                (USB_ControlRequest.wValue & 0xFF) == HID_REPORTID_DEVICE_CONTROL &&
                USB_ControlRequest.wLength == 2)
            {
                Endpoint_ClearSETUP();

                // wait until the command has been sent by the host
                while (!(Endpoint_IsOUTReceived()));

                Endpoint_Discard_8(); // discard report ID (already checked the value from SETUP transfer)
                if (Endpoint_Read_8() == ENTER_BOOTLOADER_KEY) {
                    Endpoint_ClearOUT();
                    Endpoint_ClearStatusStage();

                    // give some time for status stage to be completed
                    _delay_us(1000);
                    USB_Detach();

                    // enable the watchdog and force a timeout to reset the AVR
                    wdt_enable(WDTO_60MS);
                    for (;;);
                } else {
                    Endpoint_StallTransaction();
                }
            }

            break;
        case HID_REQ_GetProtocol:
            if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();

                /* Write the current protocol flag to the host */
                Endpoint_Write_8(UsingReportProtocol);

                Endpoint_ClearIN();
                Endpoint_ClearStatusStage();
            }

            break;
        case HID_REQ_SetProtocol:
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();
                Endpoint_ClearStatusStage();

                /* Set or clear the flag depending on what the host indicates that the current Protocol should be */
                UsingReportProtocol = (USB_ControlRequest.wValue != 0);
            }

            break;
        case HID_REQ_SetIdle:
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();
                Endpoint_ClearStatusStage();

                /* Get idle period in MSB, must multiply by 4 to get the duration in milliseconds */
                IdleCount = ((USB_ControlRequest.wValue & 0xFF00) >> 6);
            }

            break;
        case HID_REQ_GetIdle:
            if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();

                /* Write the current idle duration to the host, must be divided by 4 before sent to host */
                Endpoint_Write_8(IdleCount >> 2);

                Endpoint_ClearIN();
                Endpoint_ClearStatusStage();
            }

            break;
    }
}

/** Event handler for the USB device Start Of Frame event. */
void EVENT_USB_Device_StartOfFrame(void)
{
    /* One millisecond has elapsed, decrement the idle time remaining counter if it has not already elapsed */
    if (IdleMSRemaining)
      IdleMSRemaining--;
}

/** Fills the given HID report data structure with the next HID report to send to the host.
 *
 *  \param[out] ReportData  Pointer to a HID report data structure to be filled
 */
void CreateMouseReport(USB_MouseReport_Data_t* const ReportData)
{
    /* Clear the report contents */
    memset(ReportData, 0, sizeof(USB_MouseReport_Data_t));
    ReportData->X = 1;
}

/** Sends the next HID report to the host, via the keyboard data endpoint. */
void SendNextReport(void)
{
    static USB_MouseReport_Data_t PrevMouseReportData;
    USB_MouseReport_Data_t        MouseReportData;
    bool                          SendReport;

    /* Create the next mouse report for transmission to the host */
    CreateMouseReport(&MouseReportData);

    /* Check to see if the report data has changed - if so a report MUST be sent */
    SendReport = (memcmp(&PrevMouseReportData, &MouseReportData, sizeof(USB_MouseReport_Data_t)) != 0);

    /* Override the check if the Y or X values are non-zero - we want continuous movement while the joystick
     * is being held down (via continuous reports), otherwise the cursor will only move once per joystick toggle */
    if ((MouseReportData.Y != 0) || (MouseReportData.X != 0))
      SendReport = true;

    /* Check if the idle period is set and has elapsed */
    if (IdleCount && (!(IdleMSRemaining)))
    {
        /* Reset the idle time remaining counter */
        IdleMSRemaining = IdleCount;

        /* Idle period is set and has elapsed, must send a report to the host */
        SendReport = true;
    }

    /* Select the Mouse Report Endpoint */
    Endpoint_SelectEndpoint(MOUSE_EPADDR);

    /* Check if Mouse Endpoint Ready for Read/Write and if we should send a new report */
    if (Endpoint_IsReadWriteAllowed() && SendReport)
    {
        /* Save the current report data for later comparison to check for changes */
        PrevMouseReportData = MouseReportData;

        /* Write Mouse Report Data */
        Endpoint_Write_Stream_LE(&MouseReportData, sizeof(MouseReportData), NULL);

        /* Finalize the stream transfer to send the last packet */
        Endpoint_ClearIN();
    }
}

/** Task to manage HID report generation and transmission to the host, when in report mode. */
void Mouse_Task(void)
{
    /* Device must be connected and configured for the task to run */
    if (USB_DeviceState != DEVICE_STATE_Configured)
      return;

    /* Send the next mouse report to the host */
    SendNextReport();
}

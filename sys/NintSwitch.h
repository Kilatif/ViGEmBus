/*
* Virtual Gamepad Emulation Framework - Windows kernel-mode bus driver
* Copyright (C) 2016-2018  Benjamin Höglinger-Stelzer
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#pragma once

#define NSWITCH_DESCRIPTOR_SIZE                             0x0029
#if defined(_X86_)
#define NSWITCH_CONFIGURATION_SIZE                          0x0050
#else
#define NSWITCH_CONFIGURATION_SIZE                          0x0070
#endif
#define NSWITCH_HID_REPORT_DESCRIPTOR_SIZE                  0x00CB

#define NSWITCH_MANUFACTURER_NAME_LENGTH                    0x25
#define NSWITCH_PRODUCT_NAME_LENGTH                         0x1E
#define NSWITCH_SERIAL_NAME_LENGTH							0x1A

#define NSWITCH_REPORT_SIZE                                 0x40
#define NSWITCH_QUEUE_FLUSH_PERIOD                          0x08


//
// Nintendo Switch - specific device context data.
// 
typedef struct _NSWITCH_DEVICE_DATA
{
	UCHAR TimerStatus;
    //
    // HID Input Report buffer
    //
    UCHAR InputReport[NSWITCH_REPORT_SIZE];

    //
    // Output report cache
    //
	UCHAR OutputReport[NSWITCH_REPORT_SIZE];

    //
    // Timer for dispatching interrupt transfer
    //
    WDFTIMER PendingUsbInRequestsTimer;

    //
    // Auto-generated MAC address of the target device
    //
    MAC_ADDRESS TargetMacAddress;

    //
    // Default MAC address of the host (not used)
    //
    MAC_ADDRESS HostMacAddress;

} NSWITCH_DEVICE_DATA, *PNSWITCH_DEVICE_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NSWITCH_DEVICE_DATA, NintSwitchGetData)


EVT_WDF_TIMER NintSwitch_PendingUsbRequestsTimerFunc;

NTSTATUS
Bus_NintSwitchSubmitReport(
    WDFDEVICE Device,
    ULONG SerialNo,
    PNSWITCH_SUBMIT_REPORT Report,
    _In_ BOOLEAN FromInterface
);

//
// NSWITCH-specific functions
// 
NTSTATUS NintSwitch_PreparePdo(PWDFDEVICE_INIT DeviceInit, PUNICODE_STRING DeviceId, PUNICODE_STRING DeviceDescription);
NTSTATUS NintSwitch_PrepareHardware(WDFDEVICE Device);
NTSTATUS NintSwitch_AssignPdoContext(WDFDEVICE Device, PPDO_IDENTIFICATION_DESCRIPTION Description);
VOID NintSwitch_GetConfigurationDescriptorType(PUCHAR Buffer, ULONG Length);
VOID NintSwitch_GetDeviceDescriptorType(PUSB_DEVICE_DESCRIPTOR pDescriptor, PPDO_DEVICE_DATA pCommon);
VOID NintSwitch_SelectConfiguration(PUSBD_INTERFACE_INFORMATION pInfo);


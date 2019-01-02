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


#include "busenum.h"
#include <hidclass.h>
#include "NintSwitch.tmh"

NTSTATUS NintSwitch_PreparePdo(PWDFDEVICE_INIT DeviceInit, PUNICODE_STRING DeviceId, PUNICODE_STRING DeviceDescription)
{
    NTSTATUS status;
    UNICODE_STRING buffer;

    // prepare device description
    status = RtlUnicodeStringInit(DeviceDescription, L"Virtual Nintendo Switch Pro Controller");
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "RtlUnicodeStringInit failed with status %!STATUS!",
            status);
        return status;
    }

    // Set hardware IDs
    RtlUnicodeStringInit(&buffer, L"USB\\VID_057E&PID_2009&REV_0100");

    status = WdfPdoInitAddHardwareID(DeviceInit, &buffer);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfPdoInitAddHardwareID failed with status %!STATUS!",
            status);
        return status;
    }

    RtlUnicodeStringCopy(DeviceId, &buffer);

    RtlUnicodeStringInit(&buffer, L"USB\\VID_057E&PID_2009");

    status = WdfPdoInitAddHardwareID(DeviceInit, &buffer);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfPdoInitAddHardwareID failed with status %!STATUS!",
            status);
        return status;
    }

    // Set compatible IDs
    RtlUnicodeStringInit(&buffer, L"USB\\Class_03&SubClass_00&Prot_00");

    status = WdfPdoInitAddCompatibleID(DeviceInit, &buffer);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfPdoInitAddCompatibleID (#01) failed with status %!STATUS!",
            status);
        return status;
    }

    RtlUnicodeStringInit(&buffer, L"USB\\Class_03&SubClass_00");

    status = WdfPdoInitAddCompatibleID(DeviceInit, &buffer);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfPdoInitAddCompatibleID (#02) failed with status %!STATUS!",
            status);
        return status;
    }

    RtlUnicodeStringInit(&buffer, L"USB\\Class_03");

    status = WdfPdoInitAddCompatibleID(DeviceInit, &buffer);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfPdoInitAddCompatibleID (#03) failed with status %!STATUS!",
            status);
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS NintSwitch_PrepareHardware(WDFDEVICE Device)
{
    NTSTATUS status;
    WDF_QUERY_INTERFACE_CONFIG ifaceCfg;
    INTERFACE devinterfaceHid;

    devinterfaceHid.Size = sizeof(INTERFACE);
    devinterfaceHid.Version = 1;
    devinterfaceHid.Context = (PVOID)Device;

    devinterfaceHid.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
    devinterfaceHid.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;

    // Expose GUID_DEVINTERFACE_HID so HIDUSB can initialize
    WDF_QUERY_INTERFACE_CONFIG_INIT(&ifaceCfg, (PINTERFACE)&devinterfaceHid, &GUID_DEVINTERFACE_HID, NULL);

    status = WdfDeviceAddQueryInterface(Device, &ifaceCfg);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfDeviceAddQueryInterface failed with status %!STATUS!",
            status);
        return status;
    }

    PNSWITCH_DEVICE_DATA nintSwitchData = NintSwitchGetData(Device);
	nintSwitchData->TimerStatus = NSWITCH_TIMER_STATUS_DISABLED;
	
    // Set default HID input report (everything zero`d)
    UCHAR DefaultHidReport[NSWITCH_REPORT_SIZE] =
    {
        0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    // Initialize HID reports to defaults
    RtlCopyBytes(nintSwitchData->InputReport, DefaultHidReport, NSWITCH_REPORT_SIZE);
    RtlZeroMemory(&nintSwitchData->OutputReport, NSWITCH_REPORT_SIZE);

    // Start pending IRP queue flush timer
    WdfTimerStart(nintSwitchData->PendingUsbInRequestsTimer, NSWITCH_QUEUE_FLUSH_PERIOD);

    return STATUS_SUCCESS;
}

NTSTATUS NintSwitch_AssignPdoContext(WDFDEVICE Device, PPDO_IDENTIFICATION_DESCRIPTION Description)
{
    NTSTATUS            status;
    PNSWITCH_DEVICE_DATA    nintSwitch = NintSwitchGetData(Device);

    // Initialize periodic timer
    WDF_TIMER_CONFIG timerConfig;
    WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig, NintSwitch_PendingUsbRequestsTimerFunc, NSWITCH_QUEUE_FLUSH_PERIOD);

    // Timer object attributes
    WDF_OBJECT_ATTRIBUTES timerAttribs;
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttribs);

    // PDO is parent
    timerAttribs.ParentObject = Device;

    // Create timer
    status = WdfTimerCreate(&timerConfig, &timerAttribs, &nintSwitch->PendingUsbInRequestsTimer);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfTimerCreate failed with status %!STATUS!",
            status);
        return status;
    }

    // Load/generate MAC address

    // TODO: tidy up this region

    WDFKEY keyParams, keyTargets, keyDS, keySerial;
    UNICODE_STRING keyName, valueName;

    status = WdfDriverOpenParametersRegistryKey(WdfGetDriver(), STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &keyParams);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfDriverOpenParametersRegistryKey failed with status %!STATUS!",
            status);
        return status;
    }

    RtlUnicodeStringInit(&keyName, L"Targets");

    status = WdfRegistryCreateKey(
        keyParams,
        &keyName,
        KEY_ALL_ACCESS,
        REG_OPTION_NON_VOLATILE,
        NULL,
        WDF_NO_OBJECT_ATTRIBUTES,
        &keyTargets
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfRegistryCreateKey failed with status %!STATUS!",
            status);
        return status;
    }

    RtlUnicodeStringInit(&keyName, L"NintendoSwitchPro");

    status = WdfRegistryCreateKey(
        keyTargets,
        &keyName,
        KEY_ALL_ACCESS,
        REG_OPTION_NON_VOLATILE,
        NULL,
        WDF_NO_OBJECT_ATTRIBUTES,
        &keyDS
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfRegistryCreateKey failed with status %!STATUS!",
            status);
        return status;
    }

    DECLARE_UNICODE_STRING_SIZE(serialPath, 4);
    RtlUnicodeStringPrintf(&serialPath, L"%04d", Description->SerialNo);

    status = WdfRegistryCreateKey(
        keyDS,
        &serialPath,
        KEY_ALL_ACCESS,
        REG_OPTION_NON_VOLATILE,
        NULL,
        WDF_NO_OBJECT_ATTRIBUTES,
        &keySerial
    );
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfRegistryCreateKey failed with status %!STATUS!",
            status);
        return status;
    }

    RtlUnicodeStringInit(&valueName, L"TargetMacAddress");

    status = WdfRegistryQueryValue(keySerial, &valueName, sizeof(MAC_ADDRESS), &nintSwitch->TargetMacAddress, NULL, NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_NSWITCH,
        "MAC-Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
        nintSwitch->TargetMacAddress.Vendor0,
        nintSwitch->TargetMacAddress.Vendor1,
        nintSwitch->TargetMacAddress.Vendor2,
        nintSwitch->TargetMacAddress.Nic0,
        nintSwitch->TargetMacAddress.Nic1,
        nintSwitch->TargetMacAddress.Nic2);

    if (status == STATUS_OBJECT_NAME_NOT_FOUND)
    {
        GenerateRandomMacAddress(&nintSwitch->TargetMacAddress);

        status = WdfRegistryAssignValue(keySerial, &valueName, REG_BINARY, sizeof(MAC_ADDRESS), (PVOID)&nintSwitch->TargetMacAddress);
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_NSWITCH,
                "WdfRegistryAssignValue failed with status %!STATUS!",
                status);
            return status;
        }
    }
    else if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_NSWITCH,
            "WdfRegistryQueryValue failed with status %!STATUS!",
            status);
        return status;
    }

    WdfRegistryClose(keySerial);
    WdfRegistryClose(keyDS);
    WdfRegistryClose(keyTargets);
    WdfRegistryClose(keyParams);

    return STATUS_SUCCESS;
}

VOID NintSwitch_GetConfigurationDescriptorType(PUCHAR Buffer, ULONG Length)
{
    UCHAR NintSwitchDescriptorData[NSWITCH_DESCRIPTOR_SIZE] =
    {
        0x09,        // bLength
        0x02,        // bDescriptorType (Configuration)
        0x29, 0x00,  // wTotalLength 41
        0x01,        // bNumInterfaces 1
        0x01,        // bConfigurationValue
        0x00,        // iConfiguration (String Index)
        0xC0,        // bmAttributes Self Powered
        0xFA,        // bMaxPower 500mA

        0x09,        // bLength
        0x04,        // bDescriptorType (Interface)
        0x00,        // bInterfaceNumber 0
        0x00,        // bAlternateSetting
        0x02,        // bNumEndpoints 2
        0x03,        // bInterfaceClass
        0x00,        // bInterfaceSubClass
        0x00,        // bInterfaceProtocol
        0x00,        // iInterface (String Index)

        0x09,        // bLength
        0x21,        // bDescriptorType (HID)
        0x11, 0x01,  // bcdHID 1.11
        0x00,        // bCountryCode
        0x01,        // bNumDescriptors
        0x22,        // bDescriptorType[0] (HID)
        0xCB, 0x00,  // wDescriptorLength[0] 467

        0x07,        // bLength
        0x05,        // bDescriptorType (Endpoint)
        0x81,        // bEndpointAddress (IN/D2H)
        0x03,        // bmAttributes (Interrupt)
        0x40, 0x00,  // wMaxPacketSize 64
        0x08,        // bInterval 5 (unit depends on device speed)

        0x07,        // bLength
        0x05,        // bDescriptorType (Endpoint)
        0x01,        // bEndpointAddress (OUT/H2D)
        0x03,        // bmAttributes (Interrupt)
        0x40, 0x00,  // wMaxPacketSize 64
        0x08,        // bInterval 5 (unit depends on device speed)

                     // 41 bytes

                     // best guess: USB Standard Descriptor
    };

    RtlCopyBytes(Buffer, NintSwitchDescriptorData, Length);
}

VOID NintSwitch_GetDeviceDescriptorType(PUSB_DEVICE_DESCRIPTOR pDescriptor, PPDO_DEVICE_DATA pCommon)
{
    pDescriptor->bLength = 0x12;
    pDescriptor->bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE;
    pDescriptor->bcdUSB = 0x0200; // USB v2.0
    pDescriptor->bDeviceClass = 0x00; // per Interface
    pDescriptor->bDeviceSubClass = 0x00;
    pDescriptor->bDeviceProtocol = 0x00;
    pDescriptor->bMaxPacketSize0 = 0x40;
    pDescriptor->idVendor = pCommon->VendorId;
    pDescriptor->idProduct = pCommon->ProductId;
    pDescriptor->bcdDevice = 0x0200;
    pDescriptor->iManufacturer = 0x01;
    pDescriptor->iProduct = 0x02;
    pDescriptor->iSerialNumber = 0x03;
    pDescriptor->bNumConfigurations = 0x01;
}

VOID NintSwitch_SelectConfiguration(PUSBD_INTERFACE_INFORMATION pInfo)
{
    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_NSWITCH,
        ">> >> >> URB_FUNCTION_SELECT_CONFIGURATION: Length %d, Interface %d, Alternate %d, Pipes %d",
        (int)pInfo->Length,
        (int)pInfo->InterfaceNumber,
        (int)pInfo->AlternateSetting,
        pInfo->NumberOfPipes);

    pInfo->Class = 0x03; // HID
    pInfo->SubClass = 0x00;
    pInfo->Protocol = 0x00;

    pInfo->InterfaceHandle = (USBD_INTERFACE_HANDLE)0xFFFF0000;

    pInfo->Pipes[0].MaximumTransferSize = 0x00400000;
    pInfo->Pipes[0].MaximumPacketSize = 0x40;
    pInfo->Pipes[0].EndpointAddress = 0x81;
    pInfo->Pipes[0].Interval = 0x08;
    pInfo->Pipes[0].PipeType = 0x03;
    pInfo->Pipes[0].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0081;
    pInfo->Pipes[0].PipeFlags = 0x00;

    pInfo->Pipes[1].MaximumTransferSize = 0x00400000;
    pInfo->Pipes[1].MaximumPacketSize = 0x40;
    pInfo->Pipes[1].EndpointAddress = 0x01;
    pInfo->Pipes[1].Interval = 0x08;
    pInfo->Pipes[1].PipeType = 0x03;
    pInfo->Pipes[1].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0001;
    pInfo->Pipes[1].PipeFlags = 0x00;
}

//
// Completes pending I/O requests if feeder is too slow.
// 
VOID NintSwitch_PendingUsbRequestsTimerFunc(
    _In_ WDFTIMER Timer
)
{
    NTSTATUS                status;
    WDFREQUEST              usbRequest;
    WDFDEVICE               hChild;
    PNSWITCH_DEVICE_DATA        nintSwitchData;
    PIRP                    pendingIrp;
    PIO_STACK_LOCATION      irpStack;
    PPDO_DEVICE_DATA        pdoData;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_NSWITCH, "%!FUNC! Entry");

    hChild = WdfTimerGetParentObject(Timer);
    pdoData = PdoGetData(hChild);
    nintSwitchData = NintSwitchGetData(hChild);

	if (nintSwitchData->TimerStatus == NSWITCH_TIMER_STATUS_DISABLED)
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_NSWITCH, "%!FUNC! Exit because timer disabled");
		return;
	}

	// Get pending USB request
    status = WdfIoQueueRetrieveNextRequest(pdoData->PendingUsbInRequests, &usbRequest);

    if (NT_SUCCESS(status))
    {
        // Get pending IRP
        pendingIrp = WdfRequestWdmGetIrp(usbRequest);
        irpStack = IoGetCurrentIrpStackLocation(pendingIrp);

        // Get USB request block
        PURB urb = (PURB)irpStack->Parameters.Others.Argument1;

        // Get transfer buffer
        PUCHAR Buffer = (PUCHAR)urb->UrbBulkOrInterruptTransfer.TransferBuffer;
        // Set buffer length to report size
        urb->UrbBulkOrInterruptTransfer.TransferBufferLength = NSWITCH_REPORT_SIZE;
			           // Copy cached report to transfer buffer 
		if (Buffer)
		{
			RtlCopyBytes(Buffer, nintSwitchData->InputReport, NSWITCH_REPORT_SIZE);
		}

		        // Complete pending request
        WdfRequestComplete(usbRequest, status);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_NSWITCH, "%!FUNC! Exit with status %!STATUS!", status);
}


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
#include "usbpdo.tmh"


//
// Dummy function to satisfy USB interface
// 
BOOLEAN USB_BUSIFFN UsbPdo_IsDeviceHighSpeed(IN PVOID BusContext)
{
    UNREFERENCED_PARAMETER(BusContext);

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_USBPDO,
        "IsDeviceHighSpeed: TRUE");

    return TRUE;
}

//
// Dummy function to satisfy USB interface
// 
NTSTATUS USB_BUSIFFN UsbPdo_QueryBusInformation(
    IN PVOID BusContext,
    IN ULONG Level,
    IN OUT PVOID BusInformationBuffer,
    IN OUT PULONG BusInformationBufferLength,
    OUT PULONG BusInformationActualLength
)
{
    UNREFERENCED_PARAMETER(BusContext);
    UNREFERENCED_PARAMETER(Level);
    UNREFERENCED_PARAMETER(BusInformationBuffer);
    UNREFERENCED_PARAMETER(BusInformationBufferLength);
    UNREFERENCED_PARAMETER(BusInformationActualLength);

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_USBPDO,
        "QueryBusInformation: %!STATUS!", STATUS_UNSUCCESSFUL);

    return STATUS_UNSUCCESSFUL;
}

//
// Dummy function to satisfy USB interface
// 
NTSTATUS USB_BUSIFFN UsbPdo_SubmitIsoOutUrb(IN PVOID BusContext, IN PURB Urb)
{
    UNREFERENCED_PARAMETER(BusContext);
    UNREFERENCED_PARAMETER(Urb);

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_USBPDO,
        "SubmitIsoOutUrb: %!STATUS!", STATUS_UNSUCCESSFUL);

    return STATUS_UNSUCCESSFUL;
}

//
// Dummy function to satisfy USB interface
// 
NTSTATUS USB_BUSIFFN UsbPdo_QueryBusTime(IN PVOID BusContext, IN OUT PULONG CurrentUsbFrame)
{
    UNREFERENCED_PARAMETER(BusContext);
    UNREFERENCED_PARAMETER(CurrentUsbFrame);

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_USBPDO,
        "QueryBusTime: %!STATUS!", STATUS_UNSUCCESSFUL);

    return STATUS_UNSUCCESSFUL;
}

//
// Dummy function to satisfy USB interface
// 
VOID USB_BUSIFFN UsbPdo_GetUSBDIVersion(
    IN PVOID BusContext,
    IN OUT PUSBD_VERSION_INFORMATION VersionInformation,
    IN OUT PULONG HcdCapabilities
)
{
    UNREFERENCED_PARAMETER(BusContext);

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_USBPDO,
        "GetUSBDIVersion: 0x500, 0x200");

    if (VersionInformation != NULL)
    {
        VersionInformation->USBDI_Version = 0x500; /* Usbport */
        VersionInformation->Supported_USB_Version = 0x200; /* USB 2.0 */
    }

    if (HcdCapabilities != NULL)
    {
        *HcdCapabilities = 0;
    }
}

//
// Set device descriptor to identify the current USB device.
// 
NTSTATUS UsbPdo_GetDeviceDescriptorType(PURB urb, PPDO_DEVICE_DATA pCommon)
{
    PUSB_DEVICE_DESCRIPTOR pDescriptor = (PUSB_DEVICE_DESCRIPTOR)urb->UrbControlDescriptorRequest.TransferBuffer;

    switch (pCommon->TargetType)
    {
    case Xbox360Wired:

        Xusb_GetDeviceDescriptorType(pDescriptor, pCommon);

        break;

    case DualShock4Wired:

        Ds4_GetDeviceDescriptorType(pDescriptor, pCommon);

        break;

    case XboxOneWired:

        Xgip_GetDeviceDescriptorType(pDescriptor, pCommon);

        break;

    default:
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

//
// Set configuration descriptor, expose interfaces and endpoints.
// 
NTSTATUS UsbPdo_GetConfigurationDescriptorType(PURB urb, PPDO_DEVICE_DATA pCommon)
{
    PUCHAR Buffer = (PUCHAR)urb->UrbControlDescriptorRequest.TransferBuffer;

    // First request just gets required buffer size back
    if (urb->UrbControlDescriptorRequest.TransferBufferLength == sizeof(USB_CONFIGURATION_DESCRIPTOR))
    {
        ULONG length = sizeof(USB_CONFIGURATION_DESCRIPTOR);

        switch (pCommon->TargetType)
        {
        case Xbox360Wired:

            Xusb_GetConfigurationDescriptorType(Buffer, length);

            break;
        case DualShock4Wired:

            Ds4_GetConfigurationDescriptorType(Buffer, length);

            break;
        case XboxOneWired:

            Xgip_GetConfigurationDescriptorType(Buffer, length);

            break;
        default:
            return STATUS_UNSUCCESSFUL;
        }
    }

    ULONG length = urb->UrbControlDescriptorRequest.TransferBufferLength;

    // Second request can store the whole descriptor
    switch (pCommon->TargetType)
    {
    case Xbox360Wired:

        if (length >= XUSB_DESCRIPTOR_SIZE)
        {
            Xusb_GetConfigurationDescriptorType(Buffer, XUSB_DESCRIPTOR_SIZE);
        }

        break;
    case DualShock4Wired:

        if (length >= DS4_DESCRIPTOR_SIZE)
        {
            Ds4_GetConfigurationDescriptorType(Buffer, DS4_DESCRIPTOR_SIZE);
        }

        break;
    case XboxOneWired:

        if (length >= XGIP_DESCRIPTOR_SIZE)
        {
            Xgip_GetConfigurationDescriptorType(Buffer, XGIP_DESCRIPTOR_SIZE);
        }

        break;
    default:
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

//
// Set device string descriptors (currently only used in DS4 emulation).
// 
NTSTATUS UsbPdo_GetStringDescriptorType(PURB urb, PPDO_DEVICE_DATA pCommon)
{
    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_USBPDO,
        "Index = %d",
        urb->UrbControlDescriptorRequest.Index);

    switch (pCommon->TargetType)
    {
    case DualShock4Wired:
    {
        switch (urb->UrbControlDescriptorRequest.Index)
        {
        case 0:
        {
            // "American English"
            UCHAR LangId[HID_LANGUAGE_ID_LENGTH] =
            {
                0x04, 0x03, 0x09, 0x04
            };

            urb->UrbControlDescriptorRequest.TransferBufferLength = HID_LANGUAGE_ID_LENGTH;
            RtlCopyBytes(urb->UrbControlDescriptorRequest.TransferBuffer, LangId, HID_LANGUAGE_ID_LENGTH);

            break;
        }
        case 1:
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_USBPDO,
                "LanguageId = 0x%X",
                urb->UrbControlDescriptorRequest.LanguageId);

            if (urb->UrbControlDescriptorRequest.TransferBufferLength < DS4_MANUFACTURER_NAME_LENGTH)
            {
                PUSB_STRING_DESCRIPTOR pDesc = (PUSB_STRING_DESCRIPTOR)urb->UrbControlDescriptorRequest.TransferBuffer;
                pDesc->bLength = DS4_MANUFACTURER_NAME_LENGTH;
                break;
            }

            // "Sony Computer Entertainment"
            UCHAR ManufacturerString[DS4_MANUFACTURER_NAME_LENGTH] =
            {
				0x26, 0x03, 0x4E, 0x00, 0x69, 0x00, 0x6E, 0x00, 
            	0x74, 0x00,	0x65, 0x00, 0x6E, 0x00, 0x64, 0x00, 
            	0x6F, 0x00,	0x20, 0x00, 0x43, 0x00, 0x6F, 0x00, 
            	0x2E, 0x00,	0x2C, 0x00, 0x20, 0x00, 0x4C, 0x00, 
            	0x74, 0x00,	0x64, 0x2E
            };

            urb->UrbControlDescriptorRequest.TransferBufferLength = DS4_MANUFACTURER_NAME_LENGTH;
            RtlCopyBytes(urb->UrbControlDescriptorRequest.TransferBuffer, ManufacturerString, DS4_MANUFACTURER_NAME_LENGTH);

            break;
        }
        case 2:
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_USBPDO,
                "LanguageId = 0x%X",
                urb->UrbControlDescriptorRequest.LanguageId);

            if (urb->UrbControlDescriptorRequest.TransferBufferLength < DS4_PRODUCT_NAME_LENGTH)
            {
                PUSB_STRING_DESCRIPTOR pDesc = (PUSB_STRING_DESCRIPTOR)urb->UrbControlDescriptorRequest.TransferBuffer;
                pDesc->bLength = DS4_PRODUCT_NAME_LENGTH;
                break;
            }

            // "Wireless Controller"
            UCHAR ProductString[DS4_PRODUCT_NAME_LENGTH] =
            {
				0x1E, 0x03, 0x50, 0x00, 0x72, 0x00, 0x6f, 0x00, 
            	0x20, 0x00,	0x43, 0x00, 0x6f, 0x00, 0x6e, 0x00,
            	0x74, 0x00,	0x72, 0x00, 0x6f, 0x00, 0x6c, 0x00, 
            	0x6c, 0x00,	0x65, 0x00, 0x72, 0x00
            };

            urb->UrbControlDescriptorRequest.TransferBufferLength = DS4_PRODUCT_NAME_LENGTH;
            RtlCopyBytes(urb->UrbControlDescriptorRequest.TransferBuffer, ProductString, DS4_PRODUCT_NAME_LENGTH);

            break;
        }
		case 3:
		{
			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_USBPDO,
				"LanguageId = 0x%X",
				urb->UrbControlDescriptorRequest.LanguageId);

			if (urb->UrbControlDescriptorRequest.TransferBufferLength < DS4_SERIAL_NAME_LENGTH)
			{
				PUSB_STRING_DESCRIPTOR pDesc = (PUSB_STRING_DESCRIPTOR)urb->UrbControlDescriptorRequest.TransferBuffer;
				pDesc->bLength = DS4_SERIAL_NAME_LENGTH;
				break;
			}

			// "000000000001"
			UCHAR SerialString[DS4_SERIAL_NAME_LENGTH] =
			{
				0x1A, 0x03, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 
				0x30, 0x00,	0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 
				0x30, 0x00,	0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 
				0x31, 0x00
			};

			urb->UrbControlDescriptorRequest.TransferBufferLength = DS4_SERIAL_NAME_LENGTH;
			RtlCopyBytes(urb->UrbControlDescriptorRequest.TransferBuffer, SerialString, DS4_SERIAL_NAME_LENGTH);

			break;
		}
        default:
            break;
        }

        break;
    }
    default:
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

//
// Fakes a successfully selected configuration.
// 
NTSTATUS UsbPdo_SelectConfiguration(PURB urb, PPDO_DEVICE_DATA pCommon)
{
    PUSBD_INTERFACE_INFORMATION pInfo;

    pInfo = &urb->UrbSelectConfiguration.Interface;

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_USBPDO,
        ">> >> >> URB_FUNCTION_SELECT_CONFIGURATION: TotalLength %d",
        urb->UrbHeader.Length);

    if (urb->UrbHeader.Length == sizeof(struct _URB_SELECT_CONFIGURATION))
    {
        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_USBPDO,
            ">> >> >> URB_FUNCTION_SELECT_CONFIGURATION: NULL ConfigurationDescriptor");
        return STATUS_SUCCESS;
    }

    switch (pCommon->TargetType)
    {
    case Xbox360Wired:

        if (urb->UrbHeader.Length < XUSB_CONFIGURATION_SIZE)
        {
            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_USBPDO,
                ">> >> >> URB_FUNCTION_SELECT_CONFIGURATION: Invalid ConfigurationDescriptor");
            return STATUS_INVALID_PARAMETER;
        }

        Xusb_SelectConfiguration(pInfo);

        break;

    case DualShock4Wired:

        if (urb->UrbHeader.Length < DS4_CONFIGURATION_SIZE)
        {
            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_USBPDO,
                ">> >> >> URB_FUNCTION_SELECT_CONFIGURATION: Invalid ConfigurationDescriptor");
            return STATUS_INVALID_PARAMETER;
        }

        Ds4_SelectConfiguration(pInfo);

        break;

    case XboxOneWired:

        if (urb->UrbHeader.Length < XGIP_CONFIGURATION_SIZE)
        {
            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_USBPDO,
                ">> >> >> URB_FUNCTION_SELECT_CONFIGURATION: Invalid ConfigurationDescriptor");
            return STATUS_INVALID_PARAMETER;
        }

        Xgip_SelectConfiguration(pInfo);

        break;

    default:
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

//
// Fakes a successfully selected interface.
// 
NTSTATUS UsbPdo_SelectInterface(PURB urb, PPDO_DEVICE_DATA pCommon)
{
    PUSBD_INTERFACE_INFORMATION pInfo = &urb->UrbSelectInterface.Interface;

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_USBPDO,
        ">> >> >> URB_FUNCTION_SELECT_INTERFACE: Length %d, Interface %d, Alternate %d, Pipes %d",
        (int)pInfo->Length,
        (int)pInfo->InterfaceNumber,
        (int)pInfo->AlternateSetting,
        pInfo->NumberOfPipes);

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_USBPDO,
        ">> >> >> URB_FUNCTION_SELECT_INTERFACE: Class %d, SubClass %d, Protocol %d",
        (int)pInfo->Class,
        (int)pInfo->SubClass,
        (int)pInfo->Protocol);

    switch (pCommon->TargetType)
    {
    case Xbox360Wired:
    {
        if (pInfo->InterfaceNumber == 1)
        {
            pInfo[0].Class = 0xFF;
            pInfo[0].SubClass = 0x5D;
            pInfo[0].Protocol = 0x03;
            pInfo[0].NumberOfPipes = 0x04;

            pInfo[0].InterfaceHandle = (USBD_INTERFACE_HANDLE)0xFFFF0000;

            pInfo[0].Pipes[0].MaximumTransferSize = 0x00400000;
            pInfo[0].Pipes[0].MaximumPacketSize = 0x20;
            pInfo[0].Pipes[0].EndpointAddress = 0x82;
            pInfo[0].Pipes[0].Interval = 0x04;
            pInfo[0].Pipes[0].PipeType = 0x03;
            pInfo[0].Pipes[0].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0082;
            pInfo[0].Pipes[0].PipeFlags = 0x00;

            pInfo[0].Pipes[1].MaximumTransferSize = 0x00400000;
            pInfo[0].Pipes[1].MaximumPacketSize = 0x20;
            pInfo[0].Pipes[1].EndpointAddress = 0x02;
            pInfo[0].Pipes[1].Interval = 0x08;
            pInfo[0].Pipes[1].PipeType = 0x03;
            pInfo[0].Pipes[1].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0002;
            pInfo[0].Pipes[1].PipeFlags = 0x00;

            pInfo[0].Pipes[2].MaximumTransferSize = 0x00400000;
            pInfo[0].Pipes[2].MaximumPacketSize = 0x20;
            pInfo[0].Pipes[2].EndpointAddress = 0x83;
            pInfo[0].Pipes[2].Interval = 0x08;
            pInfo[0].Pipes[2].PipeType = 0x03;
            pInfo[0].Pipes[2].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0083;
            pInfo[0].Pipes[2].PipeFlags = 0x00;

            pInfo[0].Pipes[3].MaximumTransferSize = 0x00400000;
            pInfo[0].Pipes[3].MaximumPacketSize = 0x20;
            pInfo[0].Pipes[3].EndpointAddress = 0x03;
            pInfo[0].Pipes[3].Interval = 0x08;
            pInfo[0].Pipes[3].PipeType = 0x03;
            pInfo[0].Pipes[3].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0003;
            pInfo[0].Pipes[3].PipeFlags = 0x00;

            return STATUS_SUCCESS;
        }

        if (pInfo->InterfaceNumber == 2)
        {
            pInfo[0].Class = 0xFF;
            pInfo[0].SubClass = 0x5D;
            pInfo[0].Protocol = 0x02;
            pInfo[0].NumberOfPipes = 0x01;

            pInfo[0].InterfaceHandle = (USBD_INTERFACE_HANDLE)0xFFFF0000;

            pInfo[0].Pipes[0].MaximumTransferSize = 0x00400000;
            pInfo[0].Pipes[0].MaximumPacketSize = 0x20;
            pInfo[0].Pipes[0].EndpointAddress = 0x84;
            pInfo[0].Pipes[0].Interval = 0x04;
            pInfo[0].Pipes[0].PipeType = 0x03;
            pInfo[0].Pipes[0].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0084;
            pInfo[0].Pipes[0].PipeFlags = 0x00;

            return STATUS_SUCCESS;
        }
    }
    case DualShock4Wired:
    {
        TraceEvents(TRACE_LEVEL_WARNING,
            TRACE_USBPDO,
            "Not implemented");

        break;
    }
    default:
        break;
    }

    return STATUS_INVALID_PARAMETER;
}

//
// Dispatch interrupt transfers.
// 
NTSTATUS UsbPdo_BulkOrInterruptTransfer(PURB urb, WDFDEVICE Device, WDFREQUEST Request)
{
    struct _URB_BULK_OR_INTERRUPT_TRANSFER*     pTransfer = &urb->UrbBulkOrInterruptTransfer;
    NTSTATUS                                    status;
    PPDO_DEVICE_DATA                            pdoData;
    WDFREQUEST                                  notifyRequest;
    PUCHAR                                      blobBuffer;

    pdoData = PdoGetData(Device);

    if (pdoData == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_USBPDO,
            ">> >> >> PdoGetData failed");
        return STATUS_INVALID_PARAMETER;
    }

    switch (pdoData->TargetType)
    {
    case Xbox360Wired:
    {
        PXUSB_DEVICE_DATA xusb = XusbGetData(Device);

        // Check context
        if (xusb == NULL)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_USBPDO,
                "No XUSB context found on device %p",
                Device);

            return STATUS_UNSUCCESSFUL;
        }

        // Data coming FROM us TO higher driver
        if (pTransfer->TransferFlags & USBD_TRANSFER_DIRECTION_IN)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_USBPDO,
                ">> >> >> Incoming request, queuing...");

            blobBuffer = WdfMemoryGetBuffer(xusb->InterruptBlobStorage, NULL);

            if (XUSB_IS_DATA_PIPE(pTransfer))
            {
                //
                // Send "boot sequence" first, then the actual inputs
                // 
                switch (xusb->InterruptInitStage)
                {
                case 0:
                    pTransfer->TransferBufferLength = XUSB_INIT_STAGE_SIZE;
                    xusb->InterruptInitStage++;
                    RtlCopyMemory(
                        pTransfer->TransferBuffer, 
                        &blobBuffer[XUSB_BLOB_00_OFFSET],
                        XUSB_INIT_STAGE_SIZE
                        );
                    return STATUS_SUCCESS;
                case 1:
                    pTransfer->TransferBufferLength = XUSB_INIT_STAGE_SIZE;
                    xusb->InterruptInitStage++;
                    RtlCopyMemory(
                        pTransfer->TransferBuffer, 
                        &blobBuffer[XUSB_BLOB_01_OFFSET],
                        XUSB_INIT_STAGE_SIZE
                        );
                    return STATUS_SUCCESS;
                case 2:
                    pTransfer->TransferBufferLength = XUSB_INIT_STAGE_SIZE;
                    xusb->InterruptInitStage++;
                    RtlCopyMemory(
                        pTransfer->TransferBuffer, 
                        &blobBuffer[XUSB_BLOB_02_OFFSET],
                        XUSB_INIT_STAGE_SIZE
                        );
                    return STATUS_SUCCESS;
                case 3:
                    pTransfer->TransferBufferLength = XUSB_INIT_STAGE_SIZE;
                    xusb->InterruptInitStage++;
                    RtlCopyMemory(
                        pTransfer->TransferBuffer, 
                        &blobBuffer[XUSB_BLOB_03_OFFSET],
                        XUSB_INIT_STAGE_SIZE
                        );
                    return STATUS_SUCCESS;
                case 4:
                    pTransfer->TransferBufferLength = sizeof(XUSB_INTERRUPT_IN_PACKET);
                    xusb->InterruptInitStage++;
                    RtlCopyMemory(
                        pTransfer->TransferBuffer, 
                        &blobBuffer[XUSB_BLOB_04_OFFSET],
                        sizeof(XUSB_INTERRUPT_IN_PACKET)
                        );
                    return STATUS_SUCCESS;
                case 5:
                    pTransfer->TransferBufferLength = XUSB_INIT_STAGE_SIZE;
                    xusb->InterruptInitStage++;
                    RtlCopyMemory(
                        pTransfer->TransferBuffer, 
                        &blobBuffer[XUSB_BLOB_05_OFFSET],
                        XUSB_INIT_STAGE_SIZE
                        );
                    return STATUS_SUCCESS;
                default:
                    /* This request is sent periodically and relies on data the "feeder"
                    * has to supply, so we queue this request and return with STATUS_PENDING.
                    * The request gets completed as soon as the "feeder" sent an update. */
                    status = WdfRequestForwardToIoQueue(Request, pdoData->PendingUsbInRequests);

                    return (NT_SUCCESS(status)) ? STATUS_PENDING : status;
                }
            }

            if (XUSB_IS_CONTROL_PIPE(pTransfer))
            {
                if (!xusb->ReportedCapabilities && pTransfer->TransferBufferLength >= XUSB_INIT_STAGE_SIZE)
                {
                    RtlCopyMemory(
                        pTransfer->TransferBuffer, 
                        &blobBuffer[XUSB_BLOB_06_OFFSET],
                        XUSB_INIT_STAGE_SIZE
                        );

                    xusb->ReportedCapabilities = TRUE;

                    return STATUS_SUCCESS;
                }

                status = WdfRequestForwardToIoQueue(Request, xusb->HoldingUsbInRequests);

                return (NT_SUCCESS(status)) ? STATUS_PENDING : status;
            }
        }

        // Data coming FROM the higher driver TO us
        TraceEvents(TRACE_LEVEL_VERBOSE,
            TRACE_USBPDO,
            ">> >> >> URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER: Handle %p, Flags %X, Length %d",
            pTransfer->PipeHandle,
            pTransfer->TransferFlags,
            pTransfer->TransferBufferLength);

        if (pTransfer->TransferBufferLength == XUSB_LEDSET_SIZE) // Led
        {
            PUCHAR Buffer = pTransfer->TransferBuffer;

            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_USBPDO,
                "-- LED Buffer: %02X %02X %02X",
                Buffer[0], Buffer[1], Buffer[2]);

            // extract LED byte to get controller slot
            if (Buffer[0] == 0x01 && Buffer[1] == 0x03 && Buffer[2] >= 0x02)
            {
                if (Buffer[2] == 0x02) xusb->LedNumber = 0;
                if (Buffer[2] == 0x03) xusb->LedNumber = 1;
                if (Buffer[2] == 0x04) xusb->LedNumber = 2;
                if (Buffer[2] == 0x05) xusb->LedNumber = 3;

                TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_USBPDO,
                    "-- LED Number: %d",
                    xusb->LedNumber);
                //
                // Report back to FDO that we are ready to operate
                // 
                BUS_PDO_REPORT_STAGE_RESULT(
                    pdoData->BusInterface, 
                    ViGEmPdoInitFinished, 
                    pdoData->SerialNo, 
                    STATUS_SUCCESS
                );
            }
        }

        // Extract rumble (vibration) information
        if (pTransfer->TransferBufferLength == XUSB_RUMBLE_SIZE)
        {
            PUCHAR Buffer = pTransfer->TransferBuffer;

            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_USBPDO,
                "-- Rumble Buffer: %02X %02X %02X %02X %02X %02X %02X %02X",
                Buffer[0],
                Buffer[1],
                Buffer[2],
                Buffer[3],
                Buffer[4],
                Buffer[5],
                Buffer[6],
                Buffer[7]);

            RtlCopyBytes(xusb->Rumble, Buffer, pTransfer->TransferBufferLength);
        }

        // Notify user-mode process that new data is available
        status = WdfIoQueueRetrieveNextRequest(pdoData->PendingNotificationRequests, &notifyRequest);

        if (NT_SUCCESS(status))
        {
            PXUSB_REQUEST_NOTIFICATION notify = NULL;

            status = WdfRequestRetrieveOutputBuffer(notifyRequest, sizeof(XUSB_REQUEST_NOTIFICATION), (PVOID)&notify, NULL);

            if (NT_SUCCESS(status))
            {
                // Assign values to output buffer
                notify->Size = sizeof(XUSB_REQUEST_NOTIFICATION);
                notify->SerialNo = pdoData->SerialNo;
                notify->LedNumber = xusb->LedNumber;
                notify->LargeMotor = xusb->Rumble[3];
                notify->SmallMotor = xusb->Rumble[4];

                WdfRequestCompleteWithInformation(notifyRequest, status, notify->Size);
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_USBPDO,
                    "WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
                    status);
            }
        }

        break;
    }
    case DualShock4Wired:
    {
        PDS4_DEVICE_DATA ds4Data = Ds4GetData(Device);

        // Data coming FROM us TO higher driver
        if (pTransfer->TransferFlags & USBD_TRANSFER_DIRECTION_IN
            && pTransfer->PipeHandle == (USBD_PIPE_HANDLE)0xFFFF0081)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE,
                TRACE_USBPDO,
                ">> >> >> Incoming request, queuing...");

            /* This request is sent periodically and relies on data the "feeder"
               has to supply, so we queue this request and return with STATUS_PENDING.
               The request gets completed as soon as the "feeder" sent an update. */
            status = WdfRequestForwardToIoQueue(Request, pdoData->PendingUsbInRequests);

            return (NT_SUCCESS(status)) ? STATUS_PENDING : status;
        }

        // Store relevant bytes of buffer in PDO context
        RtlCopyBytes(&ds4Data->OutputReport,
            (PUCHAR)pTransfer->TransferBuffer + DS4_OUTPUT_BUFFER_OFFSET,
            DS4_OUTPUT_BUFFER_LENGTH);

        // Notify user-mode process that new data is available
        status = WdfIoQueueRetrieveNextRequest(pdoData->PendingNotificationRequests, &notifyRequest);

        if (NT_SUCCESS(status))
        {
            PDS4_REQUEST_NOTIFICATION notify = NULL;

            status = WdfRequestRetrieveOutputBuffer(notifyRequest, sizeof(DS4_REQUEST_NOTIFICATION), (PVOID)&notify, NULL);

            if (NT_SUCCESS(status))
            {
                // Assign values to output buffer
                notify->Size = sizeof(DS4_REQUEST_NOTIFICATION);
                notify->SerialNo = pdoData->SerialNo;
                notify->Report = ds4Data->OutputReport;

                WdfRequestCompleteWithInformation(notifyRequest, status, notify->Size);
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                    TRACE_USBPDO,
                    "WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
                    status);
            }
        }

        break;
    }
    case XboxOneWired:
    {
        PXGIP_DEVICE_DATA xgipData = XgipGetData(Device);

        // Data coming FROM us TO higher driver
        if (pTransfer->TransferFlags & USBD_TRANSFER_DIRECTION_IN)
        {
            KdPrint((DRIVERNAME ">> >> >> Incoming request, queuing..."));

            /* This request is sent periodically and relies on data the "feeder"
            has to supply, so we queue this request and return with STATUS_PENDING.
            The request gets completed as soon as the "feeder" sent an update. */
            status = WdfRequestForwardToIoQueue(Request, xgipData->PendingUsbInRequests);

            return (NT_SUCCESS(status)) ? STATUS_PENDING : status;
        }

        // Data coming FROM the higher driver TO us
        KdPrint((DRIVERNAME ">> >> >> URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER: Handle %p, Flags %X, Length %d",
            pTransfer->PipeHandle,
            pTransfer->TransferFlags,
            pTransfer->TransferBufferLength));

        break;
    }
    default:
        break;
    }

    return STATUS_SUCCESS;
}

//
// Clean-up actions on shutdown.
// 
NTSTATUS UsbPdo_AbortPipe(WDFDEVICE Device)
{
    PPDO_DEVICE_DATA pdoData = PdoGetData(Device);

    switch (pdoData->TargetType)
    {
    case DualShock4Wired:
    {
        PDS4_DEVICE_DATA ds4 = Ds4GetData(Device);

        // Check context
        if (ds4 == NULL)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                TRACE_USBPDO,
                "No DS4 context found on device %p", Device);

            return STATUS_UNSUCCESSFUL;
        }

        // Higher driver shutting down, emptying PDOs queues
        WdfTimerStop(ds4->PendingUsbInRequestsTimer, TRUE);

        break;
    }
    default:
        break;
    }

    // Higher driver shutting down, emptying PDOs queues
    WdfIoQueuePurge(pdoData->PendingUsbInRequests, NULL, NULL);
    WdfIoQueuePurge(pdoData->PendingNotificationRequests, NULL, NULL);

    return STATUS_SUCCESS;
}

//
// Processes URBs containing HID-related requests.
// 

//
// Returns interface HID report descriptor.
// 
NTSTATUS UsbPdo_GetDescriptorFromInterface(PURB urb, PPDO_DEVICE_DATA pCommon)
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;
	UCHAR Ds4HidReportDescriptor[DS4_HID_REPORT_DESCRIPTOR_SIZE] =
	{
		0x05, 0x01,                    //   Usage Page (Generic Desktop)
		0x15, 0x00,                    //   Logical Minimum (0)
		0x09, 0x04,                    //   Usage (Joystick)
		0xA1, 0x01,                    //   Collection (Application)
		0x85, 0x30,                    //   Report ID (48)
		0x05, 0x01,                    //   Usage Page (Generic Desktop)
		0x05, 0x09,                    //   Usage Page (Button)
		0x19, 0x01,                    //   Usage Minimum (Button 1)
		0x29, 0x0A,                    //   Usage Maximum (Button 10)
		0x15, 0x00,                    //   Logical Minimum (0)
		0x25, 0x01,                    //   Logical Maximum (1)
		0x75, 0x01,                    //   Report Size (1)
		0x95, 0x0A,                    //   Report Count (10)
		0x55, 0x00,                    //   Unit Exponent (0)
		0x65, 0x00,                    //   Unit (None)
		0x81, 0x02,                    //   Input (Data,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0x05, 0x09,                    //   Usage Page (Button)
		0x19, 0x0B,                    //   Usage Minimum (Button 11)
		0x29, 0x0E,                    //   Usage Maximum (Button 14)
		0x15, 0x00,                    //   Logical Minimum (0)
		0x25, 0x01,                    //   Logical Maximum (1)
		0x75, 0x01,                    //   Report Size (1)
		0x95, 0x04,                    //   Report Count (4)
		0x81, 0x02,                    //   Input (Data,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0x75, 0x01,                    //   Report Size (1)
		0x95, 0x02,                    //   Report Count (2)
		0x81, 0x03,                    //   Input (Cnst,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0x0B, 0x01, 0x00, 0x01, 0x00,  //   Usage (Generic Desktop:Pointer)
		0xA1, 0x00,                    //   Collection (Physical)
		0x0B, 0x30, 0x00, 0x01, 0x00,  //   Usage (Generic Desktop:X)
		0x0B, 0x31, 0x00, 0x01, 0x00,  //   Usage (Generic Desktop:Y)
		0x0B, 0x32, 0x00, 0x01, 0x00,  //   Usage (Generic Desktop:Z)
		0x0B, 0x35, 0x00, 0x01, 0x00,  //   Usage (Generic Desktop:Rz)
		0x15, 0x00,                    //   Logical Minimum (0)
		0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65535)
		0x75, 0x10,                    //   Report Size (16)
		0x95, 0x04,                    //   Report Count (4)
		0x81, 0x02,                    //   Input (Data,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0xC0,                          //   End Collection
		0x0B, 0x39, 0x00, 0x01, 0x00,  //   Usage (Generic Desktop:Hat Switch)
		0x15, 0x00,                    //   Logical Minimum (0)
		0x25, 0x07,                    //   Logical Maximum (7)
		0x35, 0x00,                    //   Physical Minimum (0)
		0x46, 0x3B, 0x01,              //   Physical Maximum (315)
		0x65, 0x14,                    //   Unit (Eng Rot: Degree)
		0x75, 0x04,                    //   Report Size (4)
		0x95, 0x01,                    //   Report Count (1)
		0x81, 0x02,                    //   Input (Data,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0x05, 0x09,                    //   Usage Page (Button)
		0x19, 0x0F,                    //   Usage Minimum (Button 15)
		0x29, 0x12,                    //   Usage Maximum (Button 18)
		0x15, 0x00,                    //   Logical Minimum (0)
		0x25, 0x01,                    //   Logical Maximum (1)
		0x75, 0x01,                    //   Report Size (1)
		0x95, 0x04,                    //   Report Count (4)
		0x81, 0x02,                    //   Input (Data,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0x75, 0x08,                    //   Report Size (8)
		0x95, 0x34,                    //   Report Count (52)
		0x81, 0x03,                    //   Input (Cnst,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0x06, 0x00, 0xFF,              //   Usage Page (Vendor-Defined 1)
		0x85, 0x21,                    //   Report ID (33)
		0x09, 0x01,                    //   Usage (Vendor-Defined 1)
		0x75, 0x08,                    //   Report Size (8)
		0x95, 0x3F,                    //   Report Count (63)
		0x81, 0x03,                    //   Input (Cnst,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0x85, 0x81,                    //   Report ID (129)
		0x09, 0x02,                    //   Usage (Vendor-Defined 2)
		0x75, 0x08,                    //   Report Size (8)
		0x95, 0x3F,                    //   Report Count (63)
		0x81, 0x03,                    //   Input (Cnst,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0x85, 0x01,                    //   Report ID (1)
		0x09, 0x03,                    //   Usage (Vendor-Defined 3)
		0x75, 0x08,                    //   Report Size (8)
		0x95, 0x3F,                    //   Report Count (63)
		0x91, 0x83,                    //   Output (Cnst,Var,Abs,NWrp,Lin,Pref,NNul,Vol,Bit)
		0x85, 0x10,                    //   Report ID (16)
		0x09, 0x04,                    //   Usage (Vendor-Defined 4)
		0x75, 0x08,                    //   Report Size (8)
		0x95, 0x3F,                    //   Report Count (63)
		0x91, 0x83,                    //   Output (Cnst,Var,Abs,NWrp,Lin,Pref,NNul,Vol,Bit)
		0x85, 0x80,                    //   Report ID (128)
		0x09, 0x05,                    //   Usage (Vendor-Defined 5)
		0x75, 0x08,                    //   Report Size (8)
		0x95, 0x3F,                    //   Report Count (63)
		0x91, 0x83,                    //   Output (Cnst,Var,Abs,NWrp,Lin,Pref,NNul,Vol,Bit)
		0x85, 0x82,                    //   Report ID (130)
		0x09, 0x06,                    //   Usage (Vendor-Defined 6)
		0x75, 0x08,                    //   Report Size (8)
		0x95, 0x3F,                    //   Report Count (63)
		0x91, 0x83,                    //   Output (Cnst,Var,Abs,NWrp,Lin,Pref,NNul,Vol,Bit)
		0xC0                           //   End Collection
	};

    struct _URB_CONTROL_DESCRIPTOR_REQUEST* pRequest = &urb->UrbControlDescriptorRequest;

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_USBPDO,
        ">> >> >> _URB_CONTROL_DESCRIPTOR_REQUEST: Buffer Length %d",
        pRequest->TransferBufferLength);

    switch (pCommon->TargetType)
    {
    case DualShock4Wired:
    {
        if (pRequest->TransferBufferLength >= DS4_HID_REPORT_DESCRIPTOR_SIZE)
        {
            RtlCopyMemory(pRequest->TransferBuffer, Ds4HidReportDescriptor, DS4_HID_REPORT_DESCRIPTOR_SIZE);
            status = STATUS_SUCCESS;
        }

        break;
    }
    default:
        break;
    }

    return status;
}


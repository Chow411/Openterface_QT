/*
* ========================================================================== *
*                                                                            *
*    This file is part of the Openterface Mini KVM App QT version            *
*                                                                            *
*    Copyright (C) 2024   <info@openterface.com>                             *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation version 3.                                 *
*                                                                            *
*    This program is distributed in the hope that it will be useful, but     *
*    WITHOUT ANY WARRANTY; without even the implied warranty of              *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        *
*    General Public License for more details.                                *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see <http://www.gnu.org/licenses/>.    *
*                                                                            *
* ========================================================================== *
*/

// Add Windows system headers
#ifdef _WIN32
#include <windows.h>
#include <basetsd.h>
#endif

#include "usbcontrol.h"
#include <QDebug>

// Add this line to define the logging category
Q_LOGGING_CATEGORY(log_usb, "opf.usb")

USBControl::USBControl(QObject *parent) 
    : QObject(parent)
    , context(nullptr)
    , deviceHandle(nullptr)
{
}

USBControl::~USBControl()
{
    closeUSB();
}

bool USBControl::initializeUSB()
{
    int result = libusb_init(&context);
    if (result < 0) {
        emit error(QString("Failed to initialize libusb: %1").arg(libusb_error_name(result)));
        return false;
    }
    
    return true;
}

void USBControl::closeUSB()
{
    if (deviceHandle) {
        libusb_close(deviceHandle);
        deviceHandle = nullptr;
        emit deviceDisconnected();
    }
    
    if (context) {
        libusb_exit(context);
        context = nullptr;
    }

    if (config_descriptor){
        libusb_free_config_descriptor(config_descriptor);
    }
}

bool USBControl::findAndOpenUVCDevice()
{
    // open the device with the specified VID and PID
    deviceHandle = libusb_open_device_with_vid_pid(context, VENDOR_ID, PRODUCT_ID);
    if (!deviceHandle) {
        qCDebug(log_usb) << "Failed to open device with VID:" << QString("0x%1").arg(VENDOR_ID, 4, 16, QChar('0'))
                         << "PID:" << QString("0x%1").arg(PRODUCT_ID, 4, 16, QChar('0'));
        return false;
    }
    qCDebug(log_usb) << "Successfully opened device with VID:" << QString("0x%1").arg(VENDOR_ID, 4, 16, QChar('0'))
                     << "PID:" << QString("0x%1").arg(PRODUCT_ID, 4, 16, QChar('0'));

    device = libusb_get_device(deviceHandle);
    if (!device) {
        qCDebug(log_usb) << "Failed to get device";
        return false;
    }
    qCDebug(log_usb) << "Successfully opened and configured device";

    int r = libusb_set_auto_detach_kernel_driver(deviceHandle, 1);
    if (r != 0) {
        qCDebug(log_usb) << "Failed to detach kernel driver:" << libusb_error_name(r) << libusb_strerror(r);
    } else {
        qCDebug(log_usb) << "Successfully detached the device";
    }

    int result = libusb_claim_interface(deviceHandle, 0x00);
    if (result != 0) {
        qCDebug(log_usb) << "Failed to claim interface: " << libusb_error_name(result) << libusb_strerror(result);
        return false;
    }

    getConfigDescriptor();
    showConfigDescriptor();
    // int brightness = getBrightness();
    bool init_result = initTransfer();
    if(result)
    // getContrastAsync();
    qCDebug(log_usb) << " contrast get ***************************** ";
    
    emit deviceConnected();
    return true;
}

void USBControl::getConfigDescriptor()
{
    libusb_get_config_descriptor(device, 0 ,&config_descriptor);
    // Allinterface = config_descriptor->interface;
}

void USBControl::showConfigDescriptor()
{
    qCDebug(log_usb) << "Config descriptor: ";
    qCDebug(log_usb) << "bLength: " << config_descriptor->bLength;
    qCDebug(log_usb) << "bDescriptorType: " << config_descriptor->bDescriptorType;
    qCDebug(log_usb) << "wTotalLength: " << config_descriptor->wTotalLength;
    qCDebug(log_usb) << "bNumInterfaces: " << config_descriptor->bNumInterfaces;
    qCDebug(log_usb) << "bConfigurationValue: " << config_descriptor->bConfigurationValue;
    qCDebug(log_usb) << "iConfiguration: " << config_descriptor->iConfiguration;
    qCDebug(log_usb) << "bmAttributes: " << config_descriptor->bmAttributes;
    qCDebug(log_usb) << "bMaxPower: " << config_descriptor->MaxPower;
    
    for (int i = 0; i < config_descriptor->bNumInterfaces; i++) {
        const struct libusb_interface *interface = &config_descriptor->interface[i];
        for (int j = 0; j < interface->num_altsetting; j++) {
            const struct libusb_interface_descriptor *altsetting = &interface->altsetting[j];
            qCDebug(log_usb) << "****************************************************" ;
            qCDebug(log_usb) << "Interface Number:  " << altsetting->bInterfaceNumber;
            qCDebug(log_usb) << "  bLength:" << altsetting->bLength;
            // qCDebug(log_usb) << "Interface Number:  " << altsetting->bInterfaceNumber;
            qCDebug(log_usb) << "  Alternate Setting:" << altsetting->bAlternateSetting;
            qCDebug(log_usb) << "  Interface Class:" << altsetting->bInterfaceClass;
            qCDebug(log_usb) << "  Interface Subclass:" << altsetting->bInterfaceSubClass;
            qCDebug(log_usb) << "  Interface Protocol:" << altsetting->bInterfaceProtocol;
            qCDebug(log_usb) << "  Number of Endpoints:" << altsetting->bNumEndpoints;
            
            for (int k = 0; k < altsetting->bNumEndpoints; k++) {
                const struct libusb_endpoint_descriptor *endpoint = &altsetting->endpoint[k];
                qCDebug(log_usb) << "    Endpoint Address: 0x" << QString::number(endpoint->bEndpointAddress, 16).rightJustified(2, '0');
                qCDebug(log_usb) << "    Endpoint Attributes: 0x" << QString::number(endpoint->bmAttributes, 16).rightJustified(2, '0');
                qCDebug(log_usb) << "    Max Packet Size:" << endpoint->wMaxPacketSize;
                qCDebug(log_usb) << "    Interval:" << endpoint->bInterval;
            }
            qCDebug(log_usb) << "\n" ;
        }
    }
    
}

int USBControl::getBrightness()
{
    // uint8_t data[2];
    int result;
    uint8_t 	bmRequestType = 0xA1;
    uint8_t 	bRequest = GET_CUR;
    uint16_t 	wValue = PU_BRIGHTNESS_CONTROL;
    uint16_t 	wIndex = 0x0002;
    uint8_t 	data[2];
    uint16_t 	wLength = 2;
    unsigned int 	timeout = 1000;
    result = libusb_control_transfer(
        deviceHandle,
        bmRequestType,
        bRequest,
        wValue,
        wIndex,
        data,
        wLength,
        timeout
    );
    
    // Add error handling for the control transfer
    if (result < 0) {
        qCDebug(log_usb) << "Failed to get brightness: " << libusb_strerror(result) << result;
        // Additional logging for troubleshooting
        qCDebug(log_usb) << "Control transfer parameters: "
                         << "Request Type:" << bmRequestType
                         << "Request:" << bRequest
                         << "wValue:" << wValue
                         << "wIndex:" << wIndex;
        return -1;
    }
    
    if (result != sizeof(data)) {
        qCDebug(log_usb) << "Unexpected number of bytes received for brightness: " << result;
        return -1;
    }
    
    return (data[0] << 8) | data[1];
}

void contrastTransferCallback(libusb_transfer *transfer) {
     if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        qCDebug(log_usb) << "Transfer failed:" << libusb_error_name(transfer->status);
        return;
    }

    uint8_t *data = transfer->buffer;
    uint16_t contrast = (data[1] << 8) | data[0];
    qCDebug(log_usb) << "Current Contrast: 0x" << QString::number(contrast, 16).rightJustified(4, '0');

    libusb_free_transfer(transfer);
}

void USBControl::getContrastAsync() {
    uint8_t bmRequestType = 0xA1;
    uint8_t 	bRequest = GET_CUR;
    uint16_t 	wValue = PU_BRIGHTNESS_CONTROL;
    uint16_t 	wIndex = 0x0002;
    uint16_t 	wLength = 2;
    libusb_fill_control_setup(buffer, 
                            bmRequestType,
                            bRequest,
                            wValue,
                            wIndex,
                            wLength
    );
    qCDebug(log_usb) << " After fill control setup! ";
    libusb_fill_control_transfer(contrastTransfer, deviceHandle, buffer, contrastTransferCallback, NULL, 1000);
    qCDebug(log_usb) << " After fill control tranfer! ";
    
}


bool USBControl::initTransfer(){
    contrastTransfer = libusb_alloc_transfer(0);
    return true;
}
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
    // libusb_release_interface(deviceHandle, 0);

    // int r = libusb_set_auto_detach_kernel_driver(deviceHandle, 1);
    // if (r == LIBUSB_ERROR_NOT_SUPPORTED){
    //     for (int i=0; i < 5;i++){
    //         if (libusb_kernel_driver_active(deviceHandle, i) == 1) {
    //             libusb_detach_kernel_driver(deviceHandle, i);
    //         }
    //     }
    // }else{
    //     return false;
    // }
    
    // int result = libusb_claim_interface(deviceHandle, 1);
    // if (result != 0) {
    //     qCDebug(log_usb) << "Failed to claim interface: " << libusb_error_name(result);
    //     return false;
    // }

    getConfigDescriptor();
    showConfigDescriptor();
    getDeviceDescriptor();
    int brightness = getBrightness();
    qCDebug(log_usb) << "Brightness: " << brightness;
    emit deviceConnected();
    return true;
}

void USBControl::getConfigDescriptor()
{
    libusb_get_config_descriptor(device, 0 ,&config_descriptor);
    libusb_free_config_descriptor(config_descriptor);
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

}

int USBControl::getBrightness()
{
    unsigned char data[2];
    int transferred; // To hold the number of bytes transferred
    int result;

    // Assuming you have a bulk IN endpoint for brightness control
    // Replace `BULK_IN_ENDPOINT` with the actual endpoint address
    const int BULK_IN_ENDPOINT = 0x83; // Example endpoint address, adjust as necessary

    result = libusb_bulk_transfer(
        deviceHandle,
        BULK_IN_ENDPOINT,
        data,
        sizeof(data),
        &transferred,
        1000 // Timeout in milliseconds
    );

    if (result != LIBUSB_SUCCESS) {
        qCDebug(log_usb) << "Failed to get brightness: " << libusb_error_name(result);
        return -1;
    }

    // Check if the number of bytes transferred is correct
    if (transferred != sizeof(data)) {
        qCDebug(log_usb) << "Unexpected number of bytes transferred: " << transferred;
        return -1;
    }

    return (data[0] << 8) | data[1];
}

void USBControl::getDeviceDescriptor()
{
    // Get the device descriptor
    int result = libusb_get_device_descriptor(device, &device_descriptor);
    if (result != LIBUSB_SUCCESS) {
        qCDebug(log_usb) << "Failed to get device descriptor: " << libusb_error_name(result);
        return;
    }

    // Log the device descriptor information
    qCDebug(log_usb) << "Device Descriptor:";
    qCDebug(log_usb) << "bLength: " << device_descriptor.bLength;
    qCDebug(log_usb) << "bDescriptorType: " << device_descriptor.bDescriptorType;
    qCDebug(log_usb) << "bcdUSB: " << device_descriptor.bcdUSB;
    qCDebug(log_usb) << "bDeviceClass: " << device_descriptor.bDeviceClass;
    qCDebug(log_usb) << "bDeviceSubClass: " << device_descriptor.bDeviceSubClass;
    qCDebug(log_usb) << "bDeviceProtocol: " << device_descriptor.bDeviceProtocol;
    qCDebug(log_usb) << "bMaxPacketSize0: " << device_descriptor.bMaxPacketSize0;
    qCDebug(log_usb) << "idVendor: " << QString("0x%1").arg(device_descriptor.idVendor, 4, 16, QChar('0'));
    qCDebug(log_usb) << "idProduct: " << QString("0x%1").arg(device_descriptor.idProduct, 4, 16, QChar('0'));
    qCDebug(log_usb) << "iManufacturer: " << device_descriptor.iManufacturer;
    qCDebug(log_usb) << "iProduct: " << device_descriptor.iProduct;
    qCDebug(log_usb) << "iSerialNumber: " << device_descriptor.iSerialNumber;
    qCDebug(log_usb) << "bNumConfigurations: " << device_descriptor.bNumConfigurations;
}

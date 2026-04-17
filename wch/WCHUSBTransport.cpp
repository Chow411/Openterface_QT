#include "WCHUSBTransport.h"

#include <libusb-1.0/libusb.h>

#include <cstring>
#include <sstream>
#include <iomanip>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
WCHUSBTransport::WCHUSBTransport()
{
    int rc = libusb_init(&m_ctx);
    if (rc < 0)
        throw WCHTransportError(std::string("libusb_init failed: ") +
                                libusb_error_name(rc));
}

WCHUSBTransport::~WCHUSBTransport()
{
    close();
    if (m_ctx) {
        libusb_exit(m_ctx);
        m_ctx = nullptr;
    }
}

// ---------------------------------------------------------------------------
// scanDevices
// ---------------------------------------------------------------------------
std::vector<std::string> WCHUSBTransport::scanDevices()
{
    m_scannedDevices.clear();
    std::vector<std::string> names;

    libusb_device** list = nullptr;
    ssize_t count = libusb_get_device_list(m_ctx, &list);
    if (count < 0)
        throw WCHTransportError(std::string("libusb_get_device_list failed: ") +
                                libusb_error_name(static_cast<int>(count)));

    for (ssize_t i = 0; i < count; ++i) {
        libusb_device* dev = list[i];
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(dev, &desc) < 0)
            continue;

        if ((desc.idVendor == k_vid1 || desc.idVendor == k_vid2) &&
            desc.idProduct == k_pid)
        {
            DeviceEntry entry;
            entry.vid           = desc.idVendor;
            entry.pid           = desc.idProduct;
            entry.busNumber     = libusb_get_bus_number(dev);
            entry.deviceAddress = libusb_get_device_address(dev);

            std::ostringstream oss;
            oss << "WCH ISP Device " << m_scannedDevices.size()
                << " (VID=0x" << std::hex << std::setw(4) << std::setfill('0')
                << entry.vid
                << " PID=0x" << std::setw(4) << entry.pid
                << " Bus=" << std::dec << static_cast<int>(entry.busNumber)
                << " Addr=" << static_cast<int>(entry.deviceAddress)
                << ")";

            m_scannedDevices.push_back(entry);
            names.push_back(oss.str());
        }
    }

    libusb_free_device_list(list, 1);
    return names;
}

// ---------------------------------------------------------------------------
// open
// ---------------------------------------------------------------------------
void WCHUSBTransport::open(int deviceIndex)
{
    if (m_handle)
        close();

    if (deviceIndex < 0 ||
        deviceIndex >= static_cast<int>(m_scannedDevices.size()))
        throw WCHTransportError("Invalid device index. Call scanDevices() first.");

    const DeviceEntry& entry = m_scannedDevices[static_cast<size_t>(deviceIndex)];

    libusb_device** list = nullptr;
    ssize_t count = libusb_get_device_list(m_ctx, &list);
    if (count < 0)
        throw WCHTransportError("libusb_get_device_list failed during open");

    libusb_device* target = nullptr;
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device* dev = list[i];
        if (libusb_get_bus_number(dev) == entry.busNumber &&
            libusb_get_device_address(dev) == entry.deviceAddress)
        {
            target = dev;
            break;
        }
    }

    if (!target) {
        libusb_free_device_list(list, 1);
        throw WCHTransportError("Device not found. It may have been disconnected.");
    }

    libusb_device_handle* handle = nullptr;
    int rc = libusb_open(target, &handle);
    libusb_free_device_list(list, 1);

    if (rc < 0) {
        std::string msg = std::string("libusb_open failed: ") + libusb_error_name(rc);
#ifdef _WIN32
        if (rc == LIBUSB_ERROR_NOT_SUPPORTED || rc == LIBUSB_ERROR_ACCESS) {
            msg += "\n\nOn Windows a WinUSB or libusbK driver must be installed for"
                   " this device.\nUse Zadig (https://zadig.akeo.ie) to install it:\n"
                   "  1. Open Zadig\n"
                   "  2. Select the WCH ISP device (VID 4348/1A86, PID 55E0)\n"
                   "  3. Choose WinUSB and click 'Install Driver'\n"
                   "  4. Reconnect the device and try again.";
        }
#endif
        throw WCHTransportError(msg);
    }

    m_handle = handle;

#if defined(__linux__)
    if (libusb_kernel_driver_active(m_handle, k_iface) == 1) {
        rc = libusb_detach_kernel_driver(m_handle, k_iface);
        if (rc < 0) {
            libusb_close(m_handle);
            m_handle = nullptr;
            throw WCHTransportError(std::string("Failed to detach kernel driver: ") +
                                    libusb_error_name(rc));
        }
    }
#endif

    rc = libusb_set_configuration(m_handle, 1);
    if (rc < 0 && rc != LIBUSB_ERROR_BUSY) {
        libusb_close(m_handle);
        m_handle = nullptr;
        throw WCHTransportError(std::string("libusb_set_configuration failed: ") +
                                libusb_error_name(rc));
    }

    rc = libusb_claim_interface(m_handle, k_iface);
    if (rc < 0) {
        libusb_close(m_handle);
        m_handle = nullptr;
        throw WCHTransportError(std::string("libusb_claim_interface failed: ") +
                                libusb_error_name(rc));
    }
}

// ---------------------------------------------------------------------------
// close
// ---------------------------------------------------------------------------
void WCHUSBTransport::close()
{
    if (m_handle) {
        libusb_release_interface(m_handle, k_iface);
        libusb_close(m_handle);
        m_handle = nullptr;
    }
}

// ---------------------------------------------------------------------------
// transfer — bulk OUT then bulk IN
// ---------------------------------------------------------------------------
std::vector<uint8_t> WCHUSBTransport::transfer(const std::vector<uint8_t>& command)
{
    if (!m_handle)
        throw WCHTransportError("Device not open");

    int transferred = 0;
    int rc = libusb_bulk_transfer(m_handle,
                                   k_epOut,
                                   const_cast<uint8_t*>(command.data()),
                                   static_cast<int>(command.size()),
                                   &transferred,
                                   k_timeout);
    if (rc < 0)
        throw WCHTransportError(std::string("Bulk write failed: ") +
                                libusb_error_name(rc));
    if (transferred != static_cast<int>(command.size()))
        throw WCHTransportError("Bulk write: incomplete transfer");

    std::vector<uint8_t> recvBuf(k_maxPkt, 0);
    int recvLen = 0;
    rc = libusb_bulk_transfer(m_handle,
                               k_epIn,
                               recvBuf.data(),
                               k_maxPkt,
                               &recvLen,
                               k_timeout);
    if (rc < 0)
        throw WCHTransportError(std::string("Bulk read failed: ") +
                                libusb_error_name(rc));

    recvBuf.resize(static_cast<size_t>(recvLen));
    return recvBuf;
}

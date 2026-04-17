#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>

// Forward-declare libusb types so consumers don't need the header
struct libusb_context;
struct libusb_device_handle;

// ---------------------------------------------------------------------------
// WCHTransportError
// ---------------------------------------------------------------------------
class WCHTransportError : public std::runtime_error {
public:
    explicit WCHTransportError(const std::string& msg) : std::runtime_error(msg) {}
};

// ---------------------------------------------------------------------------
// WCHUSBTransport
//
// Uses libusb-1.0 on all platforms.
//
// On Mac/Linux the OS allows user-space USB access without any special driver.
// On Windows, open() will throw with a message directing the user to install
// WinUSB via Zadig (https://zadig.akeo.ie) — a one-time, per-machine step.
//
// Target device: VID 0x4348 or 0x1A86, PID 0x55E0
// Bulk EP OUT: 0x02 | Bulk EP IN: 0x82
// ---------------------------------------------------------------------------
class WCHUSBTransport {
public:
    WCHUSBTransport();
    ~WCHUSBTransport();

    WCHUSBTransport(const WCHUSBTransport&) = delete;
    WCHUSBTransport& operator=(const WCHUSBTransport&) = delete;

    // Scan for WCH ISP devices; returns human-readable device strings.
    std::vector<std::string> scanDevices();

    // Open device by index from the last scanDevices() call.
    void open(int deviceIndex = 0);

    void close();

    bool isOpen() const { return m_handle != nullptr; }

    // Send ISP packet and receive the response.
    std::vector<uint8_t> transfer(const std::vector<uint8_t>& command);

private:
    libusb_context*       m_ctx    = nullptr;
    libusb_device_handle* m_handle = nullptr;

    struct DeviceEntry {
        uint16_t vid           = 0;
        uint16_t pid           = 0;
        uint8_t  busNumber     = 0;
        uint8_t  deviceAddress = 0;
    };
    std::vector<DeviceEntry> m_scannedDevices;

    static constexpr uint16_t k_vid1    = 0x4348;
    static constexpr uint16_t k_vid2    = 0x1A86;
    static constexpr uint16_t k_pid     = 0x55E0;
    static constexpr uint8_t  k_epOut   = 0x02;
    static constexpr uint8_t  k_epIn    = 0x82;
    static constexpr int      k_iface   = 0;
    static constexpr int      k_timeout = 5000;
    static constexpr int      k_maxPkt  = 64;
};

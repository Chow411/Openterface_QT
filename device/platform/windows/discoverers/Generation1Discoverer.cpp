#ifdef _WIN32
#include "Generation1Discoverer.h"
#include <QDebug>

Q_DECLARE_LOGGING_CATEGORY(log_device_windows)

Generation1Discoverer::Generation1Discoverer(std::shared_ptr<IDeviceEnumerator> enumerator, QObject* parent)
    : BaseDeviceDiscoverer(enumerator, parent)
{
    qCDebug(log_device_windows) << "Generation1Discoverer initialized";
}

QVector<DeviceInfo> Generation1Discoverer::discoverDevices()
{
    QVector<DeviceInfo> devices;
    
    qCDebug(log_device_windows) << "=== Generation 1 Discovery Started ===";
    qCDebug(log_device_windows) << "Looking for Original generation devices - Starting with integrated device (534D:2109)";
    
    // Phase 1: Find integrated devices first (VID_534D&PID_2109) - This is the correct original approach
    qCDebug(log_device_windows) << "Phase 1: Searching for integrated devices (534D:2109)";
    QVector<USBDeviceData> integratedDevices = findUSBDevicesWithVidPid(
        OPENTERFACE_VID, 
        OPENTERFACE_PID
    );
    qCDebug(log_device_windows) << "Found" << integratedDevices.size() << "integrated devices";
    
    for (int i = 0; i < integratedDevices.size(); ++i) {
        const USBDeviceData& integratedDevice = integratedDevices[i];
        
        qCDebug(log_device_windows) << "Processing Integrated Device" << (i + 1) << "at port chain:" << integratedDevice.portChain;
        
        DeviceInfo deviceInfo;
        deviceInfo.portChain = integratedDevice.portChain;
        deviceInfo.deviceInstanceId = integratedDevice.deviceInstanceId;
        deviceInfo.vid = OPENTERFACE_VID;
        deviceInfo.pid = OPENTERFACE_PID;
        deviceInfo.lastSeen = QDateTime::currentDateTime();
        deviceInfo.platformSpecific = integratedDevice.deviceInfo;
        deviceInfo.platformSpecific["generation"] = "Generation 1";
        
        // Store siblings and children in platformSpecific for debugging
        QVariantList siblingVariants, childrenVariants;
        for (const QVariantMap& sibling : integratedDevice.siblings) {
            siblingVariants.append(sibling);
        }
        for (const QVariantMap& child : integratedDevice.children) {
            childrenVariants.append(child);
        }
        deviceInfo.platformSpecific["siblings"] = siblingVariants;
        deviceInfo.platformSpecific["children"] = childrenVariants;
        
        // Find serial port from siblings (original approach)
        findSerialPortFromSiblings(deviceInfo, integratedDevice);
        
        // Process integrated device interfaces (camera, HID, audio from children)
        processGeneration1Interfaces(deviceInfo, integratedDevice);
        
        // Convert device IDs to real paths
        matchDevicePathsToRealPaths(deviceInfo);
        
        devices.append(deviceInfo);
        qCDebug(log_device_windows) << "Generation 1 device processing complete";
        qCDebug(log_device_windows) << "  Serial:" << (deviceInfo.hasSerialPort() ? deviceInfo.serialPortPath : "None");
        qCDebug(log_device_windows) << "  HID:" << (deviceInfo.hasHidDevice() ? "Found" : "None");
        qCDebug(log_device_windows) << "  Camera:" << (deviceInfo.hasCameraDevice() ? "Found" : "None");
        qCDebug(log_device_windows) << "  Audio:" << (deviceInfo.hasAudioDevice() ? "Found" : "None");
    }
    
    qCDebug(log_device_windows) << "=== Generation 1 Discovery Complete - Found" << devices.size() << "devices ===";
    return devices;
}

QVector<QPair<QString, QString>> Generation1Discoverer::getSupportedVidPidPairs() const
{
    return {
        {OPENTERFACE_VID, OPENTERFACE_PID},  // 534D:2109
        {SERIAL_VID, SERIAL_PID}            // 1A86:7523
    };
}

bool Generation1Discoverer::supportsVidPid(const QString& vid, const QString& pid) const
{
    auto supportedPairs = getSupportedVidPidPairs();
    for (const auto& pair : supportedPairs) {
        if (pair.first.toUpper() == vid.toUpper() && pair.second.toUpper() == pid.toUpper()) {
            return true;
        }
    }
    return false;
}

void Generation1Discoverer::processGeneration1Interfaces(DeviceInfo& deviceInfo, const USBDeviceData& integratedDevice)
{
    qCDebug(log_device_windows) << "Processing Generation 1 interfaces for integrated device:" << deviceInfo.portChain;
    
    // Process children of integrated device to find HID, camera, and audio interfaces
    processGeneration1MediaInterfaces(deviceInfo, integratedDevice);
}

void Generation1Discoverer::processGeneration1MediaInterfaces(DeviceInfo& deviceInfo, const USBDeviceData& deviceData)
{
    // Process children to find HID, camera, and audio devices - match original logic exactly
    qCDebug(log_device_windows) << "  Found" << deviceData.children.size() << "children under integrated device";
    
    // Search through integrated device's children for camera, HID, and audio
    for (const QVariantMap& child : deviceData.children) {
        QString childHardwareId = child["hardwareId"].toString();
        QString childDeviceId = child["deviceId"].toString();
        QString childClass = child["class"].toString();
        
        qCDebug(log_device_windows) << "    Integrated child - Device ID:" << childDeviceId;
        qCDebug(log_device_windows) << "      Hardware ID:" << childHardwareId;
        qCDebug(log_device_windows) << "      Class:" << childClass;
        
        // Skip interface endpoints we don't need
        if (childDeviceId.contains("&0002") || childDeviceId.contains("&0004")) {
            childDeviceId.contains("&0004");
            qCDebug(log_device_windows) << "      Skipping interface endpoint" << childDeviceId << childHardwareId;
            continue;
        }
        
        // Check for HID device (MI_04 interface) - exact original logic
        if (!deviceInfo.hasHidDevice() && (childHardwareId.toUpper().contains("HID") && childHardwareId.toUpper().contains("MI_04")) ) {
            deviceInfo.hidDeviceId = childDeviceId;
            qCDebug(log_device_windows) << "      ✓Found HID device:" << childDeviceId;
        }
        // Check for camera device (MI_00 interface) - exact original logic
        else if (!deviceInfo.hasCameraDevice() && (childHardwareId.toUpper().contains("MI_00") || childDeviceId.toUpper().contains("MI_00"))) {
            deviceInfo.cameraDeviceId = childDeviceId;
            deviceInfo.cameraDevicePath = childDeviceId;
            qCDebug(log_device_windows) << "      ✓Found camera device:" << childDeviceId;
        }
        // Check for audio device (MI_01 or Audio in hardware ID) - exact original logic
        if (!deviceInfo.hasAudioDevice() && (childHardwareId.toUpper().contains("AUDIO") ||childHardwareId.toUpper().contains("MI_01")) ) {
            deviceInfo.audioDeviceId = childDeviceId;
            qCDebug(log_device_windows) << "      ✓Found audio device:" << childDeviceId;
        }
    }
    
    qCDebug(log_device_windows) << "  Integrated device interfaces summary:";
    qCDebug(log_device_windows) << "    HID:" << (deviceInfo.hasHidDevice() ? deviceInfo.hidDeviceId : "Not found");
    qCDebug(log_device_windows) << "    Camera:" << (deviceInfo.hasCameraDevice() ? deviceInfo.cameraDeviceId : "Not found");
    qCDebug(log_device_windows) << "    Audio:" << (deviceInfo.hasAudioDevice() ? deviceInfo.audioDeviceId : "Not found");
}

void Generation1Discoverer::findSerialPortFromSiblings(DeviceInfo& deviceInfo, const USBDeviceData& integratedDevice)
{
    qCDebug(log_device_windows) << "Searching for serial port in" << integratedDevice.siblings.size() << "siblings...";
    
    for (const QVariantMap& sibling : integratedDevice.siblings) {
        QString siblingHardwareId = sibling["hardwareId"].toString();
        QString siblingDeviceId = sibling["deviceId"].toString();
        
        qCDebug(log_device_windows) << "  Checking sibling - Hardware ID:" << siblingHardwareId;
        
        // Check if this sibling is a serial port with our target VID/PID (1A86:7523)
        // This matches the original logic: Serial_vid.upper() in sibling['hardware_id'] and Serial_pid.upper() in sibling['hardware_id']
        if (siblingHardwareId.toUpper().contains(SERIAL_VID.toUpper()) &&
            siblingHardwareId.toUpper().contains(SERIAL_PID.toUpper())) {
            
            qCDebug(log_device_windows) << "  ✓Found serial port sibling:" << siblingDeviceId;
            
            deviceInfo.serialPortId = siblingDeviceId;
            // Set port chain path as per original logic - use the integrated device's port chain
            deviceInfo.serialPortPath = integratedDevice.portChain;  
            
            qCDebug(log_device_windows) << "    Serial device ID:" << siblingDeviceId;
            qCDebug(log_device_windows) << "    Device location:" << integratedDevice.portChain;
            break;
        }
    }
    
    if (deviceInfo.serialPortId.isEmpty()) {
        qCDebug(log_device_windows) << "  ⚠ No serial port sibling found with VID/PID" << SERIAL_VID << "/" << SERIAL_PID;
    }
}

#endif // _WIN32
#ifdef _WIN32
#include "Generation2Discoverer.h"
#include <QDebug>

Q_DECLARE_LOGGING_CATEGORY(log_device_discoverer)

Generation2Discoverer::Generation2Discoverer(std::shared_ptr<IDeviceEnumerator> enumerator, QObject* parent)
    : BaseDeviceDiscoverer(enumerator, parent)
{
    qCDebug(log_device_discoverer) << "Generation2Discoverer initialized";
}

QVector<DeviceInfo> Generation2Discoverer::discoverDevices()
{
    QVector<DeviceInfo> devices;
    
    qCDebug(log_device_discoverer) << "=== Generation 2 Discovery Started ===";
    qCDebug(log_device_discoverer) << "Looking for New generation USB 2.0 devices (1A86:FE0C)";
    
    // Search for New generation USB 2.0 devices (VID_1A86&PID_FE0C)
    // These behave like original generation when on USB 2.0 (use Generation 1 method)
    QVector<USBDeviceData> newGen2Devices = findUSBDevicesWithVidPid(
        SERIAL_VID_V2, 
        SERIAL_PID_V2
    );
    qCDebug(log_device_discoverer) << "Found" << newGen2Devices.size() << "New generation USB 2.0 devices";
    
    for (int i = 0; i < newGen2Devices.size(); ++i) {
        const USBDeviceData& newGen2Device = newGen2Devices[i];
        
        qCDebug(log_device_discoverer) << "Processing New Gen USB 2.0 Device" << (i + 1) << "at port chain:" << newGen2Device.portChain;
        
        DeviceInfo deviceInfo;
        deviceInfo.portChain = newGen2Device.portChain;
        deviceInfo.deviceInstanceId = newGen2Device.deviceInstanceId;
        deviceInfo.vid = SERIAL_VID_V2;
        deviceInfo.pid = SERIAL_PID_V2;
        deviceInfo.lastSeen = QDateTime::currentDateTime();
        deviceInfo.platformSpecific = newGen2Device.deviceInfo;
        
        // Store siblings and children in platformSpecific for debugging
        QVariantList siblingVariants, childrenVariants;
        for (const QVariantMap& sibling : newGen2Device.siblings) {
            siblingVariants.append(sibling);
        }
        for (const QVariantMap& child : newGen2Device.children) {
            childrenVariants.append(child);
        }
        deviceInfo.platformSpecific["siblings"] = siblingVariants;
        deviceInfo.platformSpecific["children"] = childrenVariants;
        
        // Process as Generation 1 device (integrated interfaces on USB 2.0)
        processGeneration2AsGeneration1(deviceInfo, newGen2Device);
        
        // Convert device IDs to real paths
        matchDevicePathsToRealPaths(deviceInfo);
        
        devices.append(deviceInfo);
        qCDebug(log_device_discoverer) << "Generation 2 USB 2.0 device processing complete";
        qCDebug(log_device_discoverer) << "  Serial:" << (deviceInfo.hasSerialPort() ? deviceInfo.serialPortPath : "None");
        qCDebug(log_device_discoverer) << "  HID:" << (deviceInfo.hasHidDevice() ? "Found" : "None");
        qCDebug(log_device_discoverer) << "  Camera:" << (deviceInfo.hasCameraDevice() ? "Found" : "None");
        qCDebug(log_device_discoverer) << "  Audio:" << (deviceInfo.hasAudioDevice() ? "Found" : "None");
    }
    
    qCDebug(log_device_discoverer) << "=== Generation 2 Discovery Complete - Found" << devices.size() << "devices ===";
    return devices;
}

QVector<QPair<QString, QString>> Generation2Discoverer::getSupportedVidPidPairs() const
{
    return {
        {SERIAL_VID_V2, SERIAL_PID_V2}  // 1A86:FE0C
    };
}

bool Generation2Discoverer::supportsVidPid(const QString& vid, const QString& pid) const
{
    auto supportedPairs = getSupportedVidPidPairs();
    for (const auto& pair : supportedPairs) {
        if (pair.first.toUpper() == vid.toUpper() && pair.second.toUpper() == pid.toUpper()) {
            return true;
        }
    }
    return false;
}

void Generation2Discoverer::processGeneration2AsGeneration1(DeviceInfo& deviceInfo, const USBDeviceData& gen2Device)
{
    qCDebug(log_device_discoverer) << "Processing Generation 2 device as Generation 1 (USB 2.0 compatibility)";
    
    // Set serial port information
    deviceInfo.serialPortId = gen2Device.deviceInstanceId;
    
    // Find integrated device from siblings (similar to Gen1)
    findIntegratedDeviceFromSiblings(deviceInfo, gen2Device);
}

void Generation2Discoverer::findIntegratedDeviceFromSiblings(DeviceInfo& deviceInfo, const USBDeviceData& serialDevice)
{
    qCDebug(log_device_discoverer) << "Searching for integrated device in" << serialDevice.siblings.size() << "siblings...";
    
    for (const QVariantMap& sibling : serialDevice.siblings) {
        QString siblingHardwareId = sibling["hardwareId"].toString();
        QString siblingDeviceId = sibling["deviceId"].toString();
        
        qCDebug(log_device_discoverer) << "  Checking sibling - Hardware ID:" << siblingHardwareId;
        
        // Check if this sibling is the integrated device (newer versions: 345F:2109 or 345F:2132)
        bool isIntegratedDevice = 
            (siblingHardwareId.toUpper().contains("345F") && 
             (siblingHardwareId.toUpper().contains("2109") || siblingHardwareId.toUpper().contains("2132")));
        
        if (isIntegratedDevice) {
            qCDebug(log_device_discoverer) << "Found integrated device sibling:" << siblingDeviceId;
            
            // Get the device instance for this sibling to enumerate its children
            DWORD siblingDevInst = getDeviceInstanceFromId(siblingDeviceId);
            if (siblingDevInst != 0) {
                // Get all children of this integrated device
                QVector<QVariantMap> integratedChildren = getAllChildDevices(siblingDevInst);
                qCDebug(log_device_discoverer) << "Found" << integratedChildren.size() << "children under integrated device";
                
                // Search through integrated device's children for camera, HID, and audio
                for (const QVariantMap& integratedChild : integratedChildren) {
                    QString childHardwareId = integratedChild["hardwareId"].toString();
                    QString childDeviceId = integratedChild["deviceId"].toString();
                    
                    qCDebug(log_device_discoverer) << "    Integrated child - Device ID:" << childDeviceId;
                    qCDebug(log_device_discoverer) << "      Hardware ID:" << childHardwareId;
                    
                    // Skip interface endpoints we don't need
                    if (childDeviceId.contains("&0002") || childDeviceId.contains("&0004")) {
                        continue;
                    }
                    
                    // Check for HID device (MI_04 interface)
                    if (!deviceInfo.hasHidDevice() && 
                        ((childHardwareId.toUpper().contains("HID") && childDeviceId.toUpper().contains("MI_04")) ||
                         (childDeviceId.toUpper().contains("MI_04")))) {
                        deviceInfo.hidDeviceId = childDeviceId;
                        qCDebug(log_device_discoverer) << "Found HID device:" << childDeviceId;
                    }
                    // Check for camera device (MI_00 interface)
                    else if (!deviceInfo.hasCameraDevice() && 
                             (childHardwareId.toUpper().contains("MI_00") || 
                              childDeviceId.toUpper().contains("MI_00"))) {
                        deviceInfo.cameraDeviceId = childDeviceId;
                        qCDebug(log_device_discoverer) << "Found camera device:" << childDeviceId;
                    }
                    // Check for audio device (MI_01 or Audio in hardware ID)
                    else if (!deviceInfo.hasAudioDevice() && 
                             (childHardwareId.toUpper().contains("AUDIO") ||
                              childHardwareId.toUpper().contains("MI_01") ||
                              childDeviceId.toUpper().contains("MI_01"))) {
                        deviceInfo.audioDeviceId = childDeviceId;
                        qCDebug(log_device_discoverer) << "Found audio device:" << childDeviceId;
                    }
                }
                
                qCDebug(log_device_discoverer) << "Integrated device interfaces summary:";
                qCDebug(log_device_discoverer) << "  HID:" << (deviceInfo.hasHidDevice() ? deviceInfo.hidDeviceId : "Not found");
                qCDebug(log_device_discoverer) << "  Camera:" << (deviceInfo.hasCameraDevice() ? deviceInfo.cameraDeviceId : "Not found");
                qCDebug(log_device_discoverer) << "  Audio:" << (deviceInfo.hasAudioDevice() ? deviceInfo.audioDeviceId : "Not found");
            } else {
                qCWarning(log_device_discoverer) << "Could not get device instance for integrated device sibling";
            }
            
            break; // Found the integrated device, no need to check other siblings
        }
    }
}

#endif // _WIN32
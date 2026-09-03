#ifdef _WIN32
#include "DeviceDiscoveryManager.h"
#include "device/platform/DeviceConstants.h"
#include <QDebug>
#include <QLoggingCategory>
#include <climits>

Q_DECLARE_LOGGING_CATEGORY(log_device_discoverer)

DeviceDiscoveryManager::DeviceDiscoveryManager(std::shared_ptr<IDeviceEnumerator> enumerator, QObject* parent)
    : QObject(parent), m_enumerator(enumerator)
{
    qCDebug(log_device_discoverer) << "DeviceDiscoveryManager initialized";
}

QVector<DeviceInfo> DeviceDiscoveryManager::discoverAllDevices()
{
    QVector<DeviceInfo> allDevices;
    
    qCDebug(log_device_discoverer) << "=== Starting Unified Device Discovery ===";
    qCDebug(log_device_discoverer) << "Registered discoverers:" << m_discoverers.size();
    
    // Collect devices from all discoverers
    for (auto& discoverer : m_discoverers) {
        qCDebug(log_device_discoverer) << "Running discoverer:" << discoverer->getGenerationName();
        
        QVector<DeviceInfo> devices = discoverer->discoverDevices();
        qCDebug(log_device_discoverer) << "Discoverer" << discoverer->getGenerationName() << "found" << devices.size() << "devices";
        
        // Add generation info to each device
        for (DeviceInfo& device : devices) {
            device.platformSpecific["generation"] = discoverer->getGenerationName();
            device.platformSpecific["discovererVidPidPairs"] = QVariant::fromValue(discoverer->getSupportedVidPidPairs());
        }
        
        allDevices.append(devices);
    }
    
    qCDebug(log_device_discoverer) << "Total devices found before deduplication:" << allDevices.size();
    
    // Deduplicate devices
    QVector<DeviceInfo> uniqueDevices = deduplicateDevices(allDevices);

    // Merge USB 3.0 companion devices (serial + composite on different port chains)
    uniqueDevices = mergeUsb30CompanionDevices(uniqueDevices);

    qCDebug(log_device_discoverer) << "=== Unified Discovery Complete - Found" << uniqueDevices.size() << "unique devices ===";
    
    // Log final summary
    for (int i = 0; i < uniqueDevices.size(); ++i) {
        const DeviceInfo& device = uniqueDevices[i];
        qCDebug(log_device_discoverer) << "Final Device[" << i << "]:";
        qCDebug(log_device_discoverer) << "  Generation:" << device.platformSpecific.value("generation").toString();
        qCDebug(log_device_discoverer) << "  Port Chain:" << device.portChain;
        qCDebug(log_device_discoverer) << "  VID:PID:" << device.vid << ":" << device.pid;
        qCDebug(log_device_discoverer) << "  Interfaces:" << device.getInterfaceSummary();
        qCDebug(log_device_discoverer) << "  Complete:" << (device.isCompleteDevice() ? "YES" : "NO");
    }
    
    return uniqueDevices;
}

void DeviceDiscoveryManager::registerDiscoverer(std::shared_ptr<IDeviceDiscoverer> discoverer)
{
    if (discoverer) {
        m_discoverers.append(discoverer);
        qCDebug(log_device_discoverer) << "Registered discoverer:" << discoverer->getGenerationName();
        
        auto supportedPairs = discoverer->getSupportedVidPidPairs();
        for (const auto& pair : supportedPairs) {
            qCDebug(log_device_discoverer) << "  Supports VID:PID" << pair.first << ":" << pair.second;
        }
    }
}

QVector<std::shared_ptr<IDeviceDiscoverer>> DeviceDiscoveryManager::getDiscoverers() const
{
    return m_discoverers;
}

std::shared_ptr<IDeviceDiscoverer> DeviceDiscoveryManager::getDiscovererForVidPid(const QString& vid, const QString& pid) const
{
    for (auto& discoverer : m_discoverers) {
        if (discoverer->supportsVidPid(vid, pid)) {
            return discoverer;
        }
    }
    return nullptr;
}

QVector<DeviceInfo> DeviceDiscoveryManager::deduplicateDevices(const QVector<DeviceInfo>& allDevices)
{
    QVector<DeviceInfo> uniqueDevices;
    QMap<QString, int> portChainMap; // Maps port chain to index in uniqueDevices
    
    qCDebug(log_device_discoverer) << "Deduplicating" << allDevices.size() << "devices";
    
    for (const DeviceInfo& device : allDevices) {
        QString key = device.portChain;
        
        if (portChainMap.contains(key)) {
            // Found duplicate - merge with existing device
            int existingIndex = portChainMap[key];
            DeviceInfo& existingDevice = uniqueDevices[existingIndex];
            
            qCDebug(log_device_discoverer) << "Found duplicate device at port chain:" << key;
            qCDebug(log_device_discoverer) << "  Existing:" << existingDevice.platformSpecific.value("generation").toString()
                                          << "VID:PID" << existingDevice.vid << ":" << existingDevice.pid;
            qCDebug(log_device_discoverer) << "  New:" << device.platformSpecific.value("generation").toString()
                                          << "VID:PID" << device.vid << ":" << device.pid;
            
            // Check if they're really the same device
            if (areSameDevice(existingDevice, device)) {
                qCDebug(log_device_discoverer) << "  Merging devices";
                existingDevice = mergeDeviceInfo(existingDevice, device);
            } else {
                // Different devices at same port chain (unusual but possible)
                qCDebug(log_device_discoverer) << "  Different devices at same port chain - keeping both";
                uniqueDevices.append(device);
            }
        } else {
            // New device
            portChainMap[key] = uniqueDevices.size();
            uniqueDevices.append(device);
            qCDebug(log_device_discoverer) << "Added new device at port chain:" << key
                                          << "Generation:" << device.platformSpecific.value("generation").toString();
        }
    }
    
    qCDebug(log_device_discoverer) << "Deduplication complete:" << allDevices.size() << "->" << uniqueDevices.size() << "devices";
    return uniqueDevices;
}

bool DeviceDiscoveryManager::areSameDevice(const DeviceInfo& device1, const DeviceInfo& device2)
{
    // Devices are the same if they have the same port chain and at least one compatible VID/PID combination
    if (device1.portChain != device2.portChain) {
        return false;
    }
    
    // Special case: USB 3.0 devices may have different VID/PIDs for serial vs integrated parts
    // but they should be considered the same device if they're at the same port chain
    
    // Check if both devices are from the same generation
    QString gen1 = device1.platformSpecific.value("generation").toString();
    QString gen2 = device2.platformSpecific.value("generation").toString();
    
    // If they're from different generations but same port chain, consider them the same
    // (this handles USB 2.0/3.0 compatibility cases)
    return true;
}

DeviceInfo DeviceDiscoveryManager::mergeDeviceInfo(const DeviceInfo& primary, const DeviceInfo& secondary)
{
    DeviceInfo merged = primary;
    
    // Merge interfaces - use secondary if primary doesn't have the interface
    if (merged.serialPortId.isEmpty() && !secondary.serialPortId.isEmpty()) {
        merged.serialPortId = secondary.serialPortId;
        merged.serialPortPath = secondary.serialPortPath;
        qCDebug(log_device_discoverer) << "    Merged serial port from secondary";
    }
    
    if (merged.hidDeviceId.isEmpty() && !secondary.hidDeviceId.isEmpty()) {
        merged.hidDeviceId = secondary.hidDeviceId;
        merged.hidDevicePath = secondary.hidDevicePath;
        qCDebug(log_device_discoverer) << "    Merged HID device from secondary";
    }
    
    if (merged.cameraDeviceId.isEmpty() && !secondary.cameraDeviceId.isEmpty()) {
        merged.cameraDeviceId = secondary.cameraDeviceId;
        merged.cameraDevicePath = secondary.cameraDevicePath;
        qCDebug(log_device_discoverer) << "    Merged camera device from secondary";
    }
    
    if (merged.audioDeviceId.isEmpty() && !secondary.audioDeviceId.isEmpty()) {
        merged.audioDeviceId = secondary.audioDeviceId;
        merged.audioDevicePath = secondary.audioDevicePath;
        qCDebug(log_device_discoverer) << "    Merged audio device from secondary";
    }
    
    // Merge companion device information
    if (!merged.hasCompanionDevice && secondary.hasCompanionDevice) {
        merged.hasCompanionDevice = secondary.hasCompanionDevice;
        merged.companionPortChain = secondary.companionPortChain;
        qCDebug(log_device_discoverer) << "    Merged companion device info from secondary";
    }
    
    // Merge platform-specific data
    QVariantMap secondarySpecific = secondary.platformSpecific;
    for (auto it = secondarySpecific.begin(); it != secondarySpecific.end(); ++it) {
        if (!merged.platformSpecific.contains(it.key()) || merged.platformSpecific[it.key()].isNull()) {
            merged.platformSpecific[it.key()] = it.value();
        }
    }
    
    // Update generation info to indicate it was merged
    QString mergedGeneration = merged.platformSpecific.value("generation").toString() + 
                              " + " + secondary.platformSpecific.value("generation").toString();
    merged.platformSpecific["generation"] = mergedGeneration;
    
    // Use the most recent timestamp
    if (secondary.lastSeen > merged.lastSeen) {
        merged.lastSeen = secondary.lastSeen;
    }
    
    return merged;
}

// ─────────────────────────────────────────────────────────────────────────────
// USB 3.0 companion merge — post-deduplication pass
// ─────────────────────────────────────────────────────────────────────────────

int DeviceDiscoveryManager::portChainDistance(const QString& chain1, const QString& chain2)
{
    if (chain1.isEmpty() || chain2.isEmpty()) return INT_MAX;
    if (chain1 == chain2) return 0;

    QStringList parts1 = chain1.split('-');
    QStringList parts2 = chain2.split('-');

    // Different root hubs → not comparable
    if (parts1.isEmpty() || parts2.isEmpty() || parts1.first() != parts2.first())
        return INT_MAX;

    // Compare segment by segment; count differing segments
    int maxLen = qMax(parts1.size(), parts2.size());
    int distance = 0;
    for (int i = 0; i < maxLen; ++i) {
        QString s1 = (i < parts1.size()) ? parts1[i] : QString();
        QString s2 = (i < parts2.size()) ? parts2[i] : QString();
        if (s1 != s2) ++distance;
    }
    return distance;
}

QVector<DeviceInfo> DeviceDiscoveryManager::mergeUsb30CompanionDevices(const QVector<DeviceInfo>& devices)
{
    if (devices.size() < 2) return devices;

    QVector<DeviceInfo> result = devices;
    QVector<bool> consumed(result.size(), false);

    // Classify devices into serial-only and composite-only buckets
    QList<int> serialOnlyIndices;
    QList<int> compositeOnlyIndices;

    for (int i = 0; i < result.size(); ++i) {
        const DeviceInfo& d = result[i];
        bool hasSerial = d.hasSerialPort();
        bool hasHidOrCam = d.hasHidDevice() || d.hasCameraDevice();

        if (hasSerial && !hasHidOrCam) {
            serialOnlyIndices.append(i);
        } else if (!hasSerial && hasHidOrCam) {
            compositeOnlyIndices.append(i);
        }
    }

    if (serialOnlyIndices.isEmpty() || compositeOnlyIndices.isEmpty()) {
        return result;
    }

    qCDebug(log_device_discoverer) << "USB 3.0 companion merge: candidates —"
                                   << serialOnlyIndices.size() << "serial-only,"
                                   << compositeOnlyIndices.size() << "composite-only";

    // For each serial-only device, find the best matching composite-only device.
    // On USB 3.0 systems the serial chip (1A86:FE0C) and composite device (345F:2132)
    // enumerate on DIFFERENT USB controllers (USB 2.0 vs USB 3.0 root hubs), so their
    // port chains have different root segments (e.g. "0004-..." vs "0020-...").
    // Since these VID/PID pairs are unique to KVMGO, we match by VID/PID rather than
    // requiring port chain proximity — there is at most one of each per system.
    QVector<int> mergePairs; // flat: [serialIdx, compositeIdx, ...]
    QSet<int> usedComposite;

    for (int si : serialOnlyIndices) {
        const DeviceInfo& serialDev = result[si];

        // Verify this is the KVMGO serial chip (1A86:FE0C)
        bool isKvmgoSerial = (serialDev.vid.toUpper() == SERIAL_VID_V2 && serialDev.pid.toUpper() == SERIAL_PID_V2);
        if (!isKvmgoSerial) continue;

        int bestIdx = -1;
        int bestDist = INT_MAX;

        for (int ci : compositeOnlyIndices) {
            if (usedComposite.contains(ci)) continue;
            const DeviceInfo& compDev = result[ci];

            // Verify this is the KVMGO composite device (345F:2132)
            bool isKvmgoComposite = (compDev.vid.toUpper() == OPENTERFACE_VID_V2 && compDev.pid.toUpper() == OPENTERFACE_PID_V2);
            if (!isKvmgoComposite) continue;

            // Calculate distance for tie-breaking (lower is better), but accept any pair
            int dist = portChainDistance(serialDev.portChain, compDev.portChain);
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = ci;
            }
        }

        if (bestIdx >= 0) {
            usedComposite.insert(bestIdx);
            mergePairs.append(si);
            mergePairs.append(bestIdx);
            qCDebug(log_device_discoverer) << "USB 3.0 companion match: serial"
                                           << serialDev.portChain << "↔ composite"
                                           << result[bestIdx].portChain;
        }
    }

    if (mergePairs.isEmpty()) return result;

    // Apply merges: merge composite into serial device, mark composite as consumed
    for (int pairIdx = 0; pairIdx < mergePairs.size(); pairIdx += 2) {
        int serialIdx = mergePairs[pairIdx];
        int compositeIdx = mergePairs[pairIdx + 1];

        DeviceInfo& serialDev = result[serialIdx];
        const DeviceInfo& compDev = result[compositeIdx];

        // Merge composite interfaces into the serial device
        if (serialDev.hidDeviceId.isEmpty() && !compDev.hidDeviceId.isEmpty()) {
            serialDev.hidDeviceId = compDev.hidDeviceId;
            serialDev.hidDevicePath = compDev.hidDevicePath;
            serialDev.hidVid = compDev.hidVid;
            serialDev.hidPid = compDev.hidPid;
            qCDebug(log_device_discoverer) << "    Merged HID from composite";
        }
        if (serialDev.cameraDeviceId.isEmpty() && !compDev.cameraDeviceId.isEmpty()) {
            serialDev.cameraDeviceId = compDev.cameraDeviceId;
            serialDev.cameraDevicePath = compDev.cameraDevicePath;
            qCDebug(log_device_discoverer) << "    Merged camera from composite";
        }
        if (serialDev.audioDeviceId.isEmpty() && !compDev.audioDeviceId.isEmpty()) {
            serialDev.audioDeviceId = compDev.audioDeviceId;
            serialDev.audioDevicePath = compDev.audioDevicePath;
            qCDebug(log_device_discoverer) << "    Merged audio from composite";
        }

        // Use composite device's instance ID for HID/Camera interface enumeration
        if (!compDev.deviceInstanceId.isEmpty()) {
            serialDev.deviceInstanceId = compDev.deviceInstanceId;
        }

        // Set companion relationship
        serialDev.companionPortChain = compDev.portChain;
        serialDev.hasCompanionDevice = true;

        // Mark the composite device as consumed
        consumed[compositeIdx] = true;

        serialDev.platformSpecific["generation"] =
            serialDev.platformSpecific.value("generation").toString()
            + " + USB3.0 companion merge";

        qCInfo(log_device_discoverer) << "USB 3.0 companion merge complete:"
                                      << serialDev.portChain
                                      << "↔" << compDev.portChain
                                      << "→ single device with"
                                      << serialDev.getInterfaceSummary();
    }

    // Filter out consumed devices
    QVector<DeviceInfo> finalResult;
    for (int i = 0; i < result.size(); ++i) {
        if (!consumed[i]) {
            finalResult.append(result[i]);
        }
    }

    qCDebug(log_device_discoverer) << "USB 3.0 companion merge:" << devices.size()
                                   << "→" << finalResult.size() << "devices";
    return finalResult;
}

#endif // _WIN32
#ifndef DEVICEDISCOVERYMANAGER_H
#define DEVICEDISCOVERYMANAGER_H

#include "IDeviceDiscoverer.h"
#include "device/platform//windows/IDeviceEnumerator.h"
#include <QObject>
#include <QVector>
#include <QMap>
#include <memory>

/**
 * @brief Manages device discovery across different generations
 * 
 * This class coordinates multiple device discoverers and aggregates their
 * results to provide a unified device discovery interface. It handles
 * deduplication and result merging when the same device might be detected
 * by multiple discoverers.
 */
class DeviceDiscoveryManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceDiscoveryManager(std::shared_ptr<IDeviceEnumerator> enumerator, QObject* parent = nullptr);
    
    /**
     * @brief Discover all Openterface devices across all generations
     * @return Vector of discovered devices (deduplicated)
     */
    QVector<DeviceInfo> discoverAllDevices();
    
    /**
     * @brief Register a device discoverer
     * @param discoverer Device discoverer to register
     */
    void registerDiscoverer(std::shared_ptr<IDeviceDiscoverer> discoverer);
    
    /**
     * @brief Get all registered discoverers
     * @return Vector of registered discoverers
     */
    QVector<std::shared_ptr<IDeviceDiscoverer>> getDiscoverers() const;
    
    /**
     * @brief Get discoverer that supports specific VID/PID
     * @param vid Vendor ID
     * @param pid Product ID
     * @return Discoverer that supports this VID/PID (nullptr if none)
     */
    std::shared_ptr<IDeviceDiscoverer> getDiscovererForVidPid(const QString& vid, const QString& pid) const;

private:
    /**
     * @brief Merge devices from multiple discoverers, handling duplicates
     * @param allDevices All discovered devices from all discoverers
     * @return Deduplicated device list
     */
    QVector<DeviceInfo> deduplicateDevices(const QVector<DeviceInfo>& allDevices);
    
    /**
     * @brief Check if two devices are the same (based on port chain and VID/PID)
     * @param device1 First device
     * @param device2 Second device
     * @return True if devices are considered the same
     */
    bool areSameDevice(const DeviceInfo& device1, const DeviceInfo& device2);
    
    /**
     * @brief Merge two device info objects (combine interfaces from both)
     * @param primary Primary device (result will be based on this)
     * @param secondary Secondary device (interfaces will be copied from this if missing in primary)
     * @return Merged device info
     */
    DeviceInfo mergeDeviceInfo(const DeviceInfo& primary, const DeviceInfo& secondary);

    /**
     * @brief Merge USB 3.0 companion devices that were not paired by individual discoverers
     *
     * On USB 3.0 systems the KVMGO composite device (345F:2132) and CH32V208 serial chip
     * (1A86:FE0C) may enumerate on different USB port chains. When BotherDeviceDiscoverer
     * finds only the serial side and Generation3Discoverer finds only the composite side,
     * deduplicateDevices() cannot merge them (different port chains). This method runs
     * after deduplication and pairs serial-only devices with composite-only devices by
     * USB topology proximity (same hub prefix or port chain distance <= 2).
     *
     * @param devices Deduplicated device list (modified in place)
     * @return Device list with USB 3.0 companion pairs merged
     */
    QVector<DeviceInfo> mergeUsb30CompanionDevices(const QVector<DeviceInfo>& devices);

    /**
     * @brief Calculate port chain distance between two devices
     * @param chain1 First port chain
     * @param chain2 Second port chain
     * @return Distance (number of differing segments), or INT_MAX if chains are incompatible
     */
    static int portChainDistance(const QString& chain1, const QString& chain2);

private:
    std::shared_ptr<IDeviceEnumerator> m_enumerator;
    QVector<std::shared_ptr<IDeviceDiscoverer>> m_discoverers;
};

#endif // DEVICEDISCOVERYMANAGER_H
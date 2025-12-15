#ifndef GENERATION1DISCOVERER_H
#define GENERATION1DISCOVERER_H

#include "BaseDeviceDiscoverer.h"
#include "../../DeviceConstants.h"

/**
 * @brief Generation 1 device discoverer
 * 
 * Handles discovery of original generation Openterface devices using the
 * integrated device approach. These devices typically have:
 * - VID: 534D, PID: 2109 (MS2109 integrated device)
 * - Serial port with VID: 1A86, PID: 7523 as sibling
 * - Camera, HID, and Audio as children of the integrated device
 */
class Generation1Discoverer : public BaseDeviceDiscoverer
{
    Q_OBJECT

public:
    explicit Generation1Discoverer(std::shared_ptr<IDeviceEnumerator> enumerator, QObject* parent = nullptr);
    
    // IDeviceDiscoverer interface
    QVector<DeviceInfo> discoverDevices() override;
    QString getGenerationName() const override { return "Generation 1"; }
    QVector<QPair<QString, QString>> getSupportedVidPidPairs() const override;
    bool supportsVidPid(const QString& vid, const QString& pid) const override;

private:
    /**
     * @brief Process Generation 1 device interfaces
     * @param deviceInfo Device info to populate
     * @param gen1Device USB device data for the generation 1 device
     */
    void processGeneration1Interfaces(DeviceInfo& deviceInfo, const USBDeviceData& gen1Device);
    
    /**
     * @brief Process media interfaces (HID, camera, audio) for Generation 1 device
     * @param deviceInfo Device info to update
     * @param deviceData USB device data
     */
    void processGeneration1MediaInterfaces(DeviceInfo& deviceInfo, const USBDeviceData& deviceData);
    
    /**
     * @brief Find serial port from siblings of the integrated device
     * @param deviceInfo Device info to populate
     * @param integratedDevice Integrated device data
     */
    void findSerialPortFromSiblings(DeviceInfo& deviceInfo, const USBDeviceData& integratedDevice);
};

#endif // GENERATION1DISCOVERER_H
#ifndef GENERATION2DISCOVERER_H
#define GENERATION2DISCOVERER_H

#include "BaseDeviceDiscoverer.h"
#include "../../DeviceConstants.h"

/**
 * @brief Generation 2 device discoverer
 * 
 * Handles discovery of Generation 2 Openterface devices using the
 * USB 2.0 compatibility approach. These devices may behave like Gen1
 * when on USB 2.0 or use a different discovery pattern on USB 3.0.
 */
class Generation2Discoverer : public BaseDeviceDiscoverer
{
    Q_OBJECT

public:
    explicit Generation2Discoverer(std::shared_ptr<IDeviceEnumerator> enumerator, QObject* parent = nullptr);
    
    // IDeviceDiscoverer interface
    QVector<DeviceInfo> discoverDevices() override;
    QString getGenerationName() const override { return "Generation 2 (USB 2.0)"; }
    QVector<QPair<QString, QString>> getSupportedVidPidPairs() const override;
    bool supportsVidPid(const QString& vid, const QString& pid) const override;

private:
    /**
     * @brief Process Generation 2 device interfaces when acting like Gen1
     * @param deviceInfo Device info to populate
     * @param gen2Device USB device data for the generation 2 device
     */
    void processGeneration2AsGeneration1(DeviceInfo& deviceInfo, const USBDeviceData& gen2Device);
    
    /**
     * @brief Find integrated device from siblings for Gen2 devices
     * @param deviceInfo Device info to populate
     * @param serialDevice Serial device data
     */
    void findIntegratedDeviceFromSiblings(DeviceInfo& deviceInfo, const USBDeviceData& serialDevice);
};

#endif // GENERATION2DISCOVERER_H
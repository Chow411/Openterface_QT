#ifndef CAMERAMANAGERADAPTER_H
#define CAMERAMANAGERADAPTER_H

#include <QObject>
#include <QSize>
#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QVideoWidget>
#include <QGraphicsVideoItem>
#include <QVideoFrameFormat>
#include <QSettings>
#include <memory>

// Forward declarations
class CameraManager;
class FFmpegCameraManager;

/**
 * @brief Adapter class that can switch between Qt-based and FFmpeg-based camera managers
 * 
 * This class provides a unified interface for camera management while allowing
 * the underlying implementation to be switched between Qt's native camera
 * system and our custom FFmpeg-based implementation.
 */
class CameraManagerAdapter : public QObject
{
    Q_OBJECT

public:
    enum CameraBackend {
        QtBackend,
        FFmpegBackend
    };

    explicit CameraManagerAdapter(QObject* parent = nullptr);
    ~CameraManagerAdapter();

    // Backend management
    void setCameraBackend(CameraBackend backend);
    CameraBackend currentBackend() const { return m_currentBackend; }
    
    // Camera control (unified interface)
    void startCamera();
    void stopCamera();
    bool isActive() const;
    
    // Configuration
    void setResolution(const QSize& resolution);
    void setFrameRate(int fps);
    QSize getResolution() const;
    int getFrameRate() const;
    
    // Video output
    void setVideoOutput(QGraphicsVideoItem* videoItem);
    void setVideoOutput(QVideoWidget* videoWidget);
    QGraphicsVideoItem* getVideoItem() const;
    
    // Qt Camera Manager compatibility methods
    void setCamera(const QCameraDevice &cameraDevice, QVideoWidget* videoOutput);
    void setCamera(const QCameraDevice &cameraDevice, QGraphicsVideoItem* videoOutput);
    void setCameraDevice(const QCameraDevice &cameraDevice);
    void setCameraFormat(const QCameraFormat &format);
    QCameraFormat getCameraFormat() const;
    QList<QCameraFormat> getCameraFormats() const;
    QCamera* getCamera() const;
    
    // Camera device management
    QList<QCameraDevice> getAvailableCameraDevices() const;
    QCameraDevice getCurrentCameraDevice() const;
    bool switchToCameraDevice(const QCameraDevice &cameraDevice);
    bool switchToCameraDevice(const QCameraDevice &cameraDevice, const QString& portChain);
    bool switchToCameraDeviceById(const QString& deviceId);
    QString getCurrentCameraDeviceId() const;
    QString getCurrentCameraDeviceDescription() const;
    
    // Format and capability queries
    QList<QVideoFrameFormat> getSupportedPixelFormats() const;
    QCameraFormat getVideoFormat(const QSize &resolution, int desiredFrameRate, QVideoFrameFormat::PixelFormat pixelFormat) const;
    
    // Image capture and recording
    void takeImage(const QString& file);
    void takeAreaImage(const QString& file, const QRect& captureArea);
    void startRecording();
    void stopRecording();
    
    // Initialization methods
    bool initializeCameraWithVideoOutput(QVideoWidget* videoOutput);
    bool initializeCameraWithVideoOutput(QGraphicsVideoItem* videoOutput);
    
    // Device management
    void queryResolutions();
    void configureResolutionAndFormat();
    void refreshAvailableCameraDevices();
    bool hasActiveCameraDevice() const;
    QString getCurrentCameraPortChain() const;
    bool deactivateCameraByPortChain(const QString& portChain);
    bool tryAutoSwitchToNewDevice(const QString& portChain);
    bool switchToCameraDeviceByPortChain(const QString &portChain);
    void refreshVideoOutput();
    
    // Validation methods
    bool isCameraDeviceValid(const QCameraDevice &cameraDevice) const;
    bool isCameraDeviceAvailable(const QString& deviceId) const;
    QStringList getAvailableCameraDeviceDescriptions() const;
    QStringList getAvailableCameraDeviceIds() const;
    void displayAllCameraDeviceIds() const;
    
    // Auto-detection methods
    QCameraDevice findBestAvailableCamera() const;
    QStringList getAllCameraDescriptions() const;

signals:
    // Unified signals that both backends can emit
    void cameraActiveChanged(bool active);
    void cameraSettingsApplied();
    void recordingStarted();
    void recordingStopped();
    void cameraError(const QString &errorString);
    void resolutionsUpdated(int input_width, int input_height, float input_fps, int capture_width, int capture_height, int capture_fps, float pixelClk);
    void imageCaptured(int id, const QImage& img);
    void lastImagePath(const QString& imagePath);
    void cameraDeviceChanged(const QCameraDevice& newDevice, const QCameraDevice& oldDevice);
    void cameraDeviceSwitched(const QString& fromDeviceId, const QString& toDeviceId);
    void cameraDeviceConnected(const QCameraDevice& device);
    void cameraDeviceDisconnected(const QCameraDevice& device);
    void cameraDeviceSwitching(const QString& fromDevice, const QString& toDevice);
    void cameraDeviceSwitchComplete(const QString& device);
    void availableCameraDevicesChanged(int deviceCount);
    void newDeviceAutoConnected(const QCameraDevice& device, const QString& portChain);
    
    // FFmpeg-specific signals
    void frameReady(const QVideoFrame& frame);
    void fpsChanged(double fps);
    void resolutionChanged(const QSize& resolution);

private slots:
    void onBackendCameraActiveChanged(bool active);
    void onBackendError(const QString& error);

private:
    void initializeBackend();
    void cleanupBackend();
    void connectBackendSignals();
    void disconnectBackendSignals();
    
    // Check if we should use FFmpeg backend for Linux
    bool shouldUseFFmpegBackend() const;
    
    // Backend instances
    std::unique_ptr<CameraManager> m_qtCameraManager;
    std::unique_ptr<FFmpegCameraManager> m_ffmpegCameraManager;
    
    CameraBackend m_currentBackend;
    QGraphicsVideoItem* m_videoItem;
    QVideoWidget* m_videoWidget;
    
    // Configuration storage
    QSize m_resolution;
    int m_frameRate;
    QString m_currentDeviceId;
    QCameraDevice m_currentDevice;
};

#endif // CAMERAMANAGERADAPTER_H

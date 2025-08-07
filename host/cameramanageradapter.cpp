#include "cameramanageradapter.h"
#include "cameramanager.h"
#include "ffmpegcameramanager.h"
#include <QDebug>
#include <QLoggingCategory>

#ifdef Q_OS_LINUX
#include <QSysInfo>
#endif

Q_LOGGING_CATEGORY(log_camera_adapter, "opf.camera.adapter")

CameraManagerAdapter::CameraManagerAdapter(QObject* parent)
    : QObject(parent)
    , m_currentBackend(QtBackend)
    , m_videoItem(nullptr)
    , m_videoWidget(nullptr)
    , m_resolution(1920, 1080)
    , m_frameRate(30)
{
    qCDebug(log_camera_adapter) << "CameraManagerAdapter created";
    
    // Determine the best backend for the current platform
    CameraBackend preferredBackend = shouldUseFFmpegBackend() ? FFmpegBackend : QtBackend;
    
    // Check user preference from settings
    QSettings settings("Techxartisan", "Openterface");
    QString backendSetting = settings.value("camera/backend", "auto").toString();
    
    if (backendSetting == "qt") {
        preferredBackend = QtBackend;
    } else if (backendSetting == "ffmpeg") {
        preferredBackend = FFmpegBackend;
    }
    // "auto" uses the shouldUseFFmpegBackend() result
    
    setCameraBackend(preferredBackend);
}

CameraManagerAdapter::~CameraManagerAdapter()
{
    cleanupBackend();
    qCDebug(log_camera_adapter) << "CameraManagerAdapter destroyed";
}

void CameraManagerAdapter::setCameraBackend(CameraBackend backend)
{
    if (m_currentBackend == backend && 
        ((backend == QtBackend && m_qtCameraManager) || 
         (backend == FFmpegBackend && m_ffmpegCameraManager))) {
        return;
    }
    
    qCDebug(log_camera_adapter) << "Switching camera backend to:" 
                                << (backend == QtBackend ? "Qt" : "FFmpeg");
    
    // Stop current camera if active
    bool wasActive = isActive();
    if (wasActive) {
        stopCamera();
    }
    
    // Cleanup current backend
    cleanupBackend();
    
    // Switch to new backend
    m_currentBackend = backend;
    initializeBackend();
    
    // Restore video outputs if they were set
    if (m_videoItem) {
        setVideoOutput(m_videoItem);
    } else if (m_videoWidget) {
        setVideoOutput(m_videoWidget);
    }
    
    // Restart camera if it was active
    if (wasActive) {
        startCamera();
    }
    
    qCDebug(log_camera_adapter) << "Camera backend switched successfully";
}

void CameraManagerAdapter::startCamera()
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->startCamera();
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        // For FFmpeg, we need to find an appropriate device
        QString devicePath = m_ffmpegCameraManager->findOpenterfaceCamera();
        if (devicePath.isEmpty()) {
            devicePath = "/dev/video0";
        }
        m_ffmpegCameraManager->startCamera(devicePath);
    }
}

void CameraManagerAdapter::stopCamera()
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->stopCamera();
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        m_ffmpegCameraManager->stopCamera();
    }
}

bool CameraManagerAdapter::isActive() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getCamera() && m_qtCameraManager->getCamera()->isActive();
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        return m_ffmpegCameraManager->isActive();
    }
    return false;
}

void CameraManagerAdapter::setResolution(const QSize& resolution)
{
    m_resolution = resolution;
    
    if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        m_ffmpegCameraManager->setResolution(resolution);
    }
    // For Qt backend, resolution is typically set through camera format
}

void CameraManagerAdapter::setFrameRate(int fps)
{
    m_frameRate = fps;
    
    if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        m_ffmpegCameraManager->setFrameRate(fps);
    }
    // For Qt backend, frame rate is typically set through camera format
}

QSize CameraManagerAdapter::getResolution() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        QCameraFormat format = m_qtCameraManager->getCameraFormat();
        return format.isNull() ? m_resolution : format.resolution();
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        return m_ffmpegCameraManager->resolution();
    }
    return m_resolution;
}

int CameraManagerAdapter::getFrameRate() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        QCameraFormat format = m_qtCameraManager->getCameraFormat();
        return format.isNull() ? m_frameRate : format.maxFrameRate();
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        return m_ffmpegCameraManager->frameRate();
    }
    return m_frameRate;
}

void CameraManagerAdapter::setVideoOutput(QGraphicsVideoItem* videoItem)
{
    m_videoItem = videoItem;
    m_videoWidget = nullptr;
    
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->setVideoOutput(videoItem);
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        m_ffmpegCameraManager->setVideoOutput(videoItem);
    }
}

void CameraManagerAdapter::setVideoOutput(QVideoWidget* videoWidget)
{
    m_videoWidget = videoWidget;
    m_videoItem = nullptr;
    
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->setVideoOutput(videoWidget);
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        m_ffmpegCameraManager->setVideoOutput(videoWidget);
    }
}

QGraphicsVideoItem* CameraManagerAdapter::getVideoItem() const
{
    if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        return m_ffmpegCameraManager->getVideoItem();
    }
    return m_videoItem;
}

// Qt Camera Manager compatibility methods
void CameraManagerAdapter::setCamera(const QCameraDevice &cameraDevice, QVideoWidget* videoOutput)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->setCamera(cameraDevice, videoOutput);
        m_currentDevice = cameraDevice;
    } else {
        // For FFmpeg backend, store device info and set video output
        m_currentDevice = cameraDevice;
        setVideoOutput(videoOutput);
    }
}

void CameraManagerAdapter::setCamera(const QCameraDevice &cameraDevice, QGraphicsVideoItem* videoOutput)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->setCamera(cameraDevice, videoOutput);
        m_currentDevice = cameraDevice;
    } else {
        // For FFmpeg backend, store device info and set video output
        m_currentDevice = cameraDevice;
        setVideoOutput(videoOutput);
    }
}

void CameraManagerAdapter::setCameraDevice(const QCameraDevice &cameraDevice)
{
    m_currentDevice = cameraDevice;
    
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->setCameraDevice(cameraDevice);
    }
    // FFmpeg backend doesn't use QCameraDevice directly
}

void CameraManagerAdapter::setCameraFormat(const QCameraFormat &format)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->setCameraFormat(format);
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        // Extract resolution and frame rate from format
        if (!format.isNull()) {
            setResolution(format.resolution());
            setFrameRate(format.maxFrameRate());
        }
    }
}

QCameraFormat CameraManagerAdapter::getCameraFormat() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getCameraFormat();
    }
    // For FFmpeg backend, return a mock format or empty format
    return QCameraFormat();
}

QList<QCameraFormat> CameraManagerAdapter::getCameraFormats() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getCameraFormats();
    }
    return QList<QCameraFormat>();
}

QCamera* CameraManagerAdapter::getCamera() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getCamera();
    }
    return nullptr;  // FFmpeg backend doesn't use QCamera
}

QList<QCameraDevice> CameraManagerAdapter::getAvailableCameraDevices() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getAvailableCameraDevices();
    }
    
    // For FFmpeg backend, create mock QCameraDevice objects
    QList<QCameraDevice> devices;
    if (m_ffmpegCameraManager) {
        QStringList cameras = m_ffmpegCameraManager->getAvailableCameras();
        // Note: Creating QCameraDevice from string is not straightforward
        // This would need platform-specific implementation
    }
    
    return devices;
}

QCameraDevice CameraManagerAdapter::getCurrentCameraDevice() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getCurrentCameraDevice();
    }
    return m_currentDevice;
}

bool CameraManagerAdapter::switchToCameraDevice(const QCameraDevice &cameraDevice)
{
    m_currentDevice = cameraDevice;
    
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->switchToCameraDevice(cameraDevice);
    }
    return true;  // FFmpeg backend handles device switching differently
}

bool CameraManagerAdapter::switchToCameraDevice(const QCameraDevice &cameraDevice, const QString& portChain)
{
    m_currentDevice = cameraDevice;
    
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->switchToCameraDevice(cameraDevice, portChain);
    }
    return true;
}

bool CameraManagerAdapter::switchToCameraDeviceById(const QString& deviceId)
{
    m_currentDeviceId = deviceId;
    
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->switchToCameraDeviceById(deviceId);
    }
    return true;
}

QString CameraManagerAdapter::getCurrentCameraDeviceId() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getCurrentCameraDeviceId();
    }
    return m_currentDeviceId;
}

QString CameraManagerAdapter::getCurrentCameraDeviceDescription() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getCurrentCameraDeviceDescription();
    }
    return m_currentDevice.description();
}

QList<QVideoFrameFormat> CameraManagerAdapter::getSupportedPixelFormats() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getSupportedPixelFormats();
    }
    
    // Return common formats for FFmpeg backend
    QList<QVideoFrameFormat> formats;
    QSize defaultSize(1920, 1080);
    formats.append(QVideoFrameFormat(defaultSize, QVideoFrameFormat::Format_BGRA8888));
    formats.append(QVideoFrameFormat(defaultSize, QVideoFrameFormat::Format_Jpeg));
    return formats;
}

QCameraFormat CameraManagerAdapter::getVideoFormat(const QSize &resolution, int desiredFrameRate, QVideoFrameFormat::PixelFormat pixelFormat) const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getVideoFormat(resolution, desiredFrameRate, pixelFormat);
    }
    return QCameraFormat();
}

void CameraManagerAdapter::takeImage(const QString& file)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->takeImage(file);
    }
    // FFmpeg backend would need custom image capture implementation
}

void CameraManagerAdapter::takeAreaImage(const QString& file, const QRect& captureArea)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->takeAreaImage(file, captureArea);
    }
}

void CameraManagerAdapter::startRecording()
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->startRecording();
    }
}

void CameraManagerAdapter::stopRecording()
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->stopRecording();
    }
}

bool CameraManagerAdapter::initializeCameraWithVideoOutput(QVideoWidget* videoOutput)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->initializeCameraWithVideoOutput(videoOutput);
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        setVideoOutput(videoOutput);
        return startCamera(), true;
    }
    return false;
}

bool CameraManagerAdapter::initializeCameraWithVideoOutput(QGraphicsVideoItem* videoOutput)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->initializeCameraWithVideoOutput(videoOutput);
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        setVideoOutput(videoOutput);
        startCamera();
        return true;
    }
    return false;
}

void CameraManagerAdapter::queryResolutions()
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->queryResolutions();
    }
    // FFmpeg backend handles resolution differently
}

void CameraManagerAdapter::configureResolutionAndFormat()
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->configureResolutionAndFormat();
    }
}

void CameraManagerAdapter::refreshAvailableCameraDevices()
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->refreshAvailableCameraDevices();
    }
}

bool CameraManagerAdapter::hasActiveCameraDevice() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->hasActiveCameraDevice();
    }
    return isActive();
}

QString CameraManagerAdapter::getCurrentCameraPortChain() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getCurrentCameraPortChain();
    }
    return QString();
}

bool CameraManagerAdapter::deactivateCameraByPortChain(const QString& portChain)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->deactivateCameraByPortChain(portChain);
    }
    return false;
}

bool CameraManagerAdapter::tryAutoSwitchToNewDevice(const QString& portChain)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->tryAutoSwitchToNewDevice(portChain);
    }
    return false;
}

bool CameraManagerAdapter::switchToCameraDeviceByPortChain(const QString &portChain)
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->switchToCameraDeviceByPortChain(portChain);
    }
    return false;
}

void CameraManagerAdapter::refreshVideoOutput()
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->refreshVideoOutput();
    }
}

bool CameraManagerAdapter::isCameraDeviceValid(const QCameraDevice &cameraDevice) const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->isCameraDeviceValid(cameraDevice);
    }
    return !cameraDevice.isNull();
}

bool CameraManagerAdapter::isCameraDeviceAvailable(const QString& deviceId) const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->isCameraDeviceAvailable(deviceId);
    }
    return false;
}

QStringList CameraManagerAdapter::getAvailableCameraDeviceDescriptions() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getAvailableCameraDeviceDescriptions();
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        return m_ffmpegCameraManager->getAvailableCameras();
    }
    return QStringList();
}

QStringList CameraManagerAdapter::getAvailableCameraDeviceIds() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getAvailableCameraDeviceIds();
    }
    return QStringList();
}

void CameraManagerAdapter::displayAllCameraDeviceIds() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        m_qtCameraManager->displayAllCameraDeviceIds();
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        QStringList cameras = m_ffmpegCameraManager->getAvailableCameras();
        qCDebug(log_camera_adapter) << "Available cameras (FFmpeg backend):" << cameras;
    }
}

QCameraDevice CameraManagerAdapter::findBestAvailableCamera() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->findBestAvailableCamera();
    }
    return QCameraDevice();
}

QStringList CameraManagerAdapter::getAllCameraDescriptions() const
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        return m_qtCameraManager->getAllCameraDescriptions();
    }
    return getAvailableCameraDeviceDescriptions();
}

void CameraManagerAdapter::onBackendCameraActiveChanged(bool active)
{
    emit cameraActiveChanged(active);
}

void CameraManagerAdapter::onBackendError(const QString& error)
{
    emit cameraError(error);
}

void CameraManagerAdapter::initializeBackend()
{
    if (m_currentBackend == QtBackend) {
        m_qtCameraManager = std::make_unique<CameraManager>(this);
        connectBackendSignals();
        qCDebug(log_camera_adapter) << "Qt camera backend initialized";
    } else if (m_currentBackend == FFmpegBackend) {
        m_ffmpegCameraManager = std::make_unique<FFmpegCameraManager>(this);
        connectBackendSignals();
        qCDebug(log_camera_adapter) << "FFmpeg camera backend initialized";
    }
}

void CameraManagerAdapter::cleanupBackend()
{
    disconnectBackendSignals();
    
    if (m_qtCameraManager) {
        m_qtCameraManager.reset();
    }
    
    if (m_ffmpegCameraManager) {
        m_ffmpegCameraManager.reset();
    }
}

void CameraManagerAdapter::connectBackendSignals()
{
    if (m_currentBackend == QtBackend && m_qtCameraManager) {
        // Connect Qt camera manager signals
        connect(m_qtCameraManager.get(), &CameraManager::cameraActiveChanged,
                this, &CameraManagerAdapter::onBackendCameraActiveChanged);
        connect(m_qtCameraManager.get(), &CameraManager::cameraError,
                this, &CameraManagerAdapter::onBackendError);
        connect(m_qtCameraManager.get(), &CameraManager::cameraSettingsApplied,
                this, &CameraManagerAdapter::cameraSettingsApplied);
        connect(m_qtCameraManager.get(), &CameraManager::recordingStarted,
                this, &CameraManagerAdapter::recordingStarted);
        connect(m_qtCameraManager.get(), &CameraManager::recordingStopped,
                this, &CameraManagerAdapter::recordingStopped);
        connect(m_qtCameraManager.get(), &CameraManager::resolutionsUpdated,
                this, &CameraManagerAdapter::resolutionsUpdated);
        connect(m_qtCameraManager.get(), &CameraManager::imageCaptured,
                this, &CameraManagerAdapter::imageCaptured);
        connect(m_qtCameraManager.get(), &CameraManager::lastImagePath,
                this, &CameraManagerAdapter::lastImagePath);
        connect(m_qtCameraManager.get(), &CameraManager::cameraDeviceChanged,
                this, &CameraManagerAdapter::cameraDeviceChanged);
        connect(m_qtCameraManager.get(), &CameraManager::cameraDeviceSwitched,
                this, &CameraManagerAdapter::cameraDeviceSwitched);
        connect(m_qtCameraManager.get(), &CameraManager::cameraDeviceConnected,
                this, &CameraManagerAdapter::cameraDeviceConnected);
        connect(m_qtCameraManager.get(), &CameraManager::cameraDeviceDisconnected,
                this, &CameraManagerAdapter::cameraDeviceDisconnected);
        connect(m_qtCameraManager.get(), &CameraManager::cameraDeviceSwitching,
                this, &CameraManagerAdapter::cameraDeviceSwitching);
        connect(m_qtCameraManager.get(), &CameraManager::cameraDeviceSwitchComplete,
                this, &CameraManagerAdapter::cameraDeviceSwitchComplete);
        connect(m_qtCameraManager.get(), &CameraManager::availableCameraDevicesChanged,
                this, &CameraManagerAdapter::availableCameraDevicesChanged);
        connect(m_qtCameraManager.get(), &CameraManager::newDeviceAutoConnected,
                this, &CameraManagerAdapter::newDeviceAutoConnected);
                
    } else if (m_currentBackend == FFmpegBackend && m_ffmpegCameraManager) {
        // Connect FFmpeg camera manager signals
        connect(m_ffmpegCameraManager.get(), &FFmpegCameraManager::cameraActiveChanged,
                this, &CameraManagerAdapter::onBackendCameraActiveChanged);
        connect(m_ffmpegCameraManager.get(), &FFmpegCameraManager::error,
                this, &CameraManagerAdapter::onBackendError);
        connect(m_ffmpegCameraManager.get(), &FFmpegCameraManager::frameReady,
                this, &CameraManagerAdapter::frameReady);
        connect(m_ffmpegCameraManager.get(), &FFmpegCameraManager::fpsChanged,
                this, &CameraManagerAdapter::fpsChanged);
        connect(m_ffmpegCameraManager.get(), &FFmpegCameraManager::resolutionChanged,
                this, &CameraManagerAdapter::resolutionChanged);
    }
}

void CameraManagerAdapter::disconnectBackendSignals()
{
    // Disconnect all signals from current backend
    if (m_qtCameraManager) {
        disconnect(m_qtCameraManager.get(), nullptr, this, nullptr);
    }
    
    if (m_ffmpegCameraManager) {
        disconnect(m_ffmpegCameraManager.get(), nullptr, this, nullptr);
    }
}

bool CameraManagerAdapter::shouldUseFFmpegBackend() const
{
#ifdef Q_OS_LINUX
    // On Linux, prefer FFmpeg backend for better performance with V4L2 devices
    return true;
#else
    // On other platforms, use Qt backend
    return false;
#endif
}

#include "cameramanageradapter.moc"

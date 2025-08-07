#include "ffmpegintegratecameramanager.h"
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QVideoFrameFormat>
#include <QImage>
#include <QMediaCaptureSession>
#include "video/videohid.h"
#include "../ui/globalsetting.h"

Q_LOGGING_CATEGORY(log_ffmpeg_integrate, "opf.ffmpeg.integrate")

FFmpegIntegrateCameraManager::FFmpegIntegrateCameraManager(QObject* parent)
    : QObject(parent)
    , m_videoItem(nullptr)
    , m_videoWidget(nullptr)
    , m_resolution(1920, 1080)
    , m_fps(30)
    , m_isActive(false)
    , m_frameProcessTimer(new QTimer(this))
    , m_performanceTimer(new QTimer(this))
    , m_currentFPS(0.0)
    , m_totalFramesProcessed(0)
    , m_droppedFrames(0)
    , m_takeSnapshotNext(false)
    , m_takeAreaSnapshotNext(false)
{
    qCDebug(log_ffmpeg_integrate) << "FFmpegIntegrateCameraManager created";
    
    // Initialize capture thread
    m_captureThread = std::make_unique<FFmpegCameraThread>(this);
    
    // Connect capture thread signals
    connect(m_captureThread.get(), &FFmpegCameraThread::frameReady,
            this, &FFmpegIntegrateCameraManager::onFrameReady);
    connect(m_captureThread.get(), &FFmpegCameraThread::error,
            this, &FFmpegIntegrateCameraManager::onCaptureError);
    connect(m_captureThread.get(), &FFmpegCameraThread::fpsChanged,
            this, &FFmpegIntegrateCameraManager::onFPSChanged);
    connect(m_captureThread.get(), &FFmpegCameraThread::captureStarted,
            this, [this]() {
                m_isActive = true;
                emit cameraActiveChanged(true);
                qCDebug(log_ffmpeg_integrate) << "Camera capture started";
            });
    connect(m_captureThread.get(), &FFmpegCameraThread::captureStopped,
            this, [this]() {
                m_isActive = false;
                emit cameraActiveChanged(false);
                qCDebug(log_ffmpeg_integrate) << "Camera capture stopped";
            });
    
    // Setup frame processing timer
    m_frameProcessTimer->setSingleShot(false);
    m_frameProcessTimer->setInterval(16); // ~60 FPS processing rate
    connect(m_frameProcessTimer, &QTimer::timeout,
            this, &FFmpegIntegrateCameraManager::processFrameQueue);
    
    // Setup performance monitoring timer
    m_performanceTimer->setSingleShot(false);
    m_performanceTimer->setInterval(1000); // Update every second
    connect(m_performanceTimer, &QTimer::timeout,
            this, &FFmpegIntegrateCameraManager::updatePerformanceInfo);
    
    // Initialize video sink
    initializeVideoSink();
}

FFmpegIntegrateCameraManager::~FFmpegIntegrateCameraManager()
{
    stopCamera();
    cleanupVideoSink();
    qCDebug(log_ffmpeg_integrate) << "FFmpegIntegrateCameraManager destroyed";
}

bool FFmpegIntegrateCameraManager::startCamera(const QString& devicePath)
{
    if (m_isActive) {
        qCWarning(log_ffmpeg_integrate) << "Camera already active";
        return true;
    }
    
    // Determine device path
    QString actualDevicePath = devicePath;
    if (actualDevicePath.isEmpty()) {
        actualDevicePath = findBestCamera();
    }
    
    if (actualDevicePath.isEmpty()) {
        qCWarning(log_ffmpeg_integrate) << "No camera device found";
        emit error("No camera device found");
        return false;
    }
    
    m_currentDevice = actualDevicePath;
    
    qCDebug(log_ffmpeg_integrate) << "Starting camera:" << actualDevicePath 
                                   << "resolution:" << m_resolution << "fps:" << m_fps;
    
    // Configure capture thread
    m_captureThread->setResolution(m_resolution);
    m_captureThread->setFrameRate(m_fps);
    
    // Start capture
    bool success = m_captureThread->startCapture(actualDevicePath, m_resolution, m_fps);
    if (success) {
        m_frameProcessTimer->start();
        m_performanceTimer->start();
        
        // Reset performance counters
        m_totalFramesProcessed = 0;
        m_droppedFrames = 0;
    }
    
    return success;
}

void FFmpegIntegrateCameraManager::stopCamera()
{
    if (!m_isActive) {
        return;
    }
    
    qCDebug(log_ffmpeg_integrate) << "Stopping camera";
    
    m_frameProcessTimer->stop();
    m_performanceTimer->stop();
    
    if (m_captureThread) {
        m_captureThread->stopCapture();
    }
    
    // Clear frame queue
    {
        QMutexLocker locker(&m_frameQueueMutex);
        m_frameQueue.clear();
    }
    
    m_currentDevice.clear();
}

bool FFmpegIntegrateCameraManager::isActive() const
{
    return m_isActive && m_captureThread && m_captureThread->isCapturing();
}

void FFmpegIntegrateCameraManager::setResolution(const QSize& resolution)
{
    if (m_resolution == resolution) {
        return;
    }
    
    m_resolution = resolution;
    
    if (m_captureThread) {
        m_captureThread->setResolution(resolution);
    }
    
    emit resolutionChanged(resolution);
    qCDebug(log_ffmpeg_integrate) << "Resolution changed to:" << resolution;
}

void FFmpegIntegrateCameraManager::setFrameRate(int fps)
{
    if (m_fps == fps) {
        return;
    }
    
    m_fps = fps;
    
    if (m_captureThread) {
        m_captureThread->setFrameRate(fps);
    }
    
    qCDebug(log_ffmpeg_integrate) << "Frame rate changed to:" << fps;
}

QSize FFmpegIntegrateCameraManager::resolution() const
{
    return m_resolution;
}

int FFmpegIntegrateCameraManager::frameRate() const
{
    return m_fps;
}

void FFmpegIntegrateCameraManager::setVideoOutput(QGraphicsVideoItem* videoItem)
{
    if (m_videoItem == videoItem) {
        return;
    }
    
    m_videoItem = videoItem;
    m_videoWidget = nullptr; // Clear widget output
    
    if (m_videoItem) {
        // In Qt6, we need to present frames directly to the video sink
        m_videoSink.reset(m_videoItem->videoSink());
        qCDebug(log_ffmpeg_integrate) << "Video output set to QGraphicsVideoItem";
    }
}

void FFmpegIntegrateCameraManager::setVideoOutput(QVideoWidget* videoWidget)
{
    if (m_videoWidget == videoWidget) {
        return;
    }
    
    m_videoWidget = videoWidget;
    m_videoItem = nullptr; // Clear graphics item output
    
    if (m_videoWidget) {
        // In Qt6, we need to present frames directly to the video sink
        m_videoSink.reset(m_videoWidget->videoSink());
        qCDebug(log_ffmpeg_integrate) << "Video output set to QVideoWidget";
    }
}

QStringList FFmpegIntegrateCameraManager::getAvailableCameras() const
{
    return FFmpegCameraThread::getAvailableV4L2Devices();
}

QString FFmpegIntegrateCameraManager::findBestCamera() const
{
    // First try to find Openterface device
    QString openterfaceDevice = FFmpegCameraThread::findOpenterfaceDevice();
    if (!openterfaceDevice.isEmpty()) {
        return openterfaceDevice;
    }
    
    // Fallback to first available camera
    QStringList cameras = getAvailableCameras();
    return cameras.isEmpty() ? "/dev/video0" : cameras.first();
}

QList<QSize> FFmpegIntegrateCameraManager::getSupportedResolutions() const
{
    if (m_currentDevice.isEmpty()) {
        // Return common resolutions as default
        return {QSize(640, 480), QSize(1280, 720), QSize(1920, 1080)};
    }
    
    return FFmpegCameraThread::getSupportedResolutions(m_currentDevice);
}

QList<int> FFmpegIntegrateCameraManager::getSupportedFrameRates() const
{
    // Return common frame rates
    return {15, 24, 25, 30, 50, 60};
}

double FFmpegIntegrateCameraManager::getCurrentFPS() const
{
    QMutexLocker locker(&m_performanceMutex);
    return m_currentFPS;
}

int FFmpegIntegrateCameraManager::getDroppedFrames() const
{
    QMutexLocker locker(&m_performanceMutex);
    return m_droppedFrames + (m_captureThread ? m_captureThread->getDroppedFrameCount() : 0);
}

QString FFmpegIntegrateCameraManager::getPerformanceInfo() const
{
    QMutexLocker locker(&m_performanceMutex);
    
    int captureDropped = m_captureThread ? m_captureThread->getDroppedFrameCount() : 0;
    int queueSize = m_captureThread ? m_captureThread->getBufferSize() : 0;
    
    return QString("FPS: %1 | Frames: %2 | Dropped: %3 | Queue: %4")
           .arg(m_currentFPS, 0, 'f', 1)
           .arg(m_totalFramesProcessed)
           .arg(m_droppedFrames + captureDropped)
           .arg(queueSize);
}

void FFmpegIntegrateCameraManager::takeSnapshot(const QString& filePath)
{
    m_snapshotPath = filePath.isEmpty() ? generateSnapshotPath() : filePath;
    m_takeSnapshotNext = true;
    
    qCDebug(log_ffmpeg_integrate) << "Snapshot scheduled:" << m_snapshotPath;
}

void FFmpegIntegrateCameraManager::takeAreaSnapshot(const QString& filePath, const QRect& area)
{
    m_snapshotPath = filePath.isEmpty() ? generateSnapshotPath() : filePath;
    m_snapshotArea = area;
    m_takeAreaSnapshotNext = true;
    
    qCDebug(log_ffmpeg_integrate) << "Area snapshot scheduled:" << m_snapshotPath << "area:" << area;
}

void FFmpegIntegrateCameraManager::onFrameReady(const QVideoFrame& frame)
{
    if (!frame.isValid()) {
        return;
    }
    
    // Add frame to processing queue
    {
        QMutexLocker locker(&m_frameQueueMutex);
        
        // Limit queue size to prevent memory buildup
        while (m_frameQueue.size() >= MAX_FRAME_QUEUE_SIZE) {
            m_frameQueue.dequeue();
            m_droppedFrames++;
        }
        
        m_frameQueue.enqueue(frame);
    }
    
    emit frameReady(frame);
}

void FFmpegIntegrateCameraManager::onCaptureError(const QString& error)
{
    qCWarning(log_ffmpeg_integrate) << "Capture error:" << error;
    emit this->error(error);
}

void FFmpegIntegrateCameraManager::onFPSChanged(double fps)
{
    {
        QMutexLocker locker(&m_performanceMutex);
        m_currentFPS = fps;
    }
    
    emit fpsChanged(fps);
}

void FFmpegIntegrateCameraManager::processFrameQueue()
{
    QVideoFrame frame;
    
    // Get next frame from queue
    {
        QMutexLocker locker(&m_frameQueueMutex);
        if (m_frameQueue.isEmpty()) {
            return;
        }
        frame = m_frameQueue.dequeue();
    }
    
    if (!frame.isValid()) {
        return;
    }
    
    // Handle snapshot requests
    if (m_takeSnapshotNext || m_takeAreaSnapshotNext) {
        if (frame.map(QVideoFrame::ReadOnly)) {
            QImage image = frame.toImage();
            if (!image.isNull()) {
                if (m_takeAreaSnapshotNext && !m_snapshotArea.isEmpty()) {
                    // Crop image to specified area
                    QRect cropRect = m_snapshotArea.intersected(image.rect());
                    if (!cropRect.isEmpty()) {
                        image = image.copy(cropRect);
                    }
                }
                
                if (image.save(m_snapshotPath)) {
                    emit snapshotSaved(m_snapshotPath);
                    qCDebug(log_ffmpeg_integrate) << "Snapshot saved:" << m_snapshotPath;
                } else {
                    qCWarning(log_ffmpeg_integrate) << "Failed to save snapshot:" << m_snapshotPath;
                }
            }
            frame.unmap();
        }
        
        m_takeSnapshotNext = false;
        m_takeAreaSnapshotNext = false;
    }
    
    // Send frame to video sink for display
    if (m_videoSink) {
        m_videoSink->setVideoFrame(frame);
    }
    
    m_totalFramesProcessed++;
}

void FFmpegIntegrateCameraManager::updatePerformanceInfo()
{
    QString info = getPerformanceInfo();
    emit performanceUpdated(info);
    
    qCDebug(log_ffmpeg_integrate) << "Performance:" << info;
}

void FFmpegIntegrateCameraManager::initializeVideoSink()
{
    m_videoSink = std::make_unique<QVideoSink>(this);
    qCDebug(log_ffmpeg_integrate) << "Video sink initialized";
}

void FFmpegIntegrateCameraManager::cleanupVideoSink()
{
    if (m_videoSink) {
        m_videoSink.reset();
        qCDebug(log_ffmpeg_integrate) << "Video sink cleaned up";
    }
}

QString FFmpegIntegrateCameraManager::generateSnapshotPath() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString openterfacePath = picturesPath + "/Openterface";
    
    QDir dir(openterfacePath);
    if (!dir.exists()) {
        dir.mkpath(openterfacePath);
    }
    
    return openterfacePath + "/openterface_" + timestamp + ".png";
}

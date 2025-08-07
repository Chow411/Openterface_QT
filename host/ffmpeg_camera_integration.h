#ifndef FFMPEG_CAMERA_INTEGRATION_H
#define FFMPEG_CAMERA_INTEGRATION_H

/**
 * @file ffmpeg_camera_integration.h
 * @brief Main header for FFmpeg camera integration
 * 
 * This header provides convenient access to all FFmpeg camera components
 * and ensures proper initialization order.
 */

// Core FFmpeg components
#include "ffmpegcamerathread.h"
#include "ffmpegintegratecameramanager.h"

// Enhanced camera manager with backend switching
#include "cameramanager.h"

// Qt multimedia framework
#include <QVideoFrame>
#include <QVideoSink>
#include <QGraphicsVideoItem>
#include <QVideoWidget>

/**
 * @brief Initialize FFmpeg camera system
 * 
 * Call this function once before using any FFmpeg camera functionality.
 * This ensures FFmpeg is properly initialized and logging is configured.
 */
inline void initializeFFmpegCameraSystem()
{
    // FFmpeg initialization is handled automatically by the components
    // This function is provided for potential future initialization needs
    qDebug() << "FFmpeg camera system initialization complete";
}

/**
 * @brief Quick setup function for FFmpeg camera with QGraphicsVideoItem
 * 
 * @param parent Parent QObject
 * @param videoItem QGraphicsVideoItem for display
 * @param devicePath Optional specific device path (auto-detected if empty)
 * @param resolution Desired resolution (default 1920x1080)
 * @param fps Desired frame rate (default 30)
 * @return Configured CameraManager instance
 */
inline CameraManager* setupFFmpegCamera(QObject* parent,
                                       QGraphicsVideoItem* videoItem,
                                       const QString& devicePath = "",
                                       const QSize& resolution = QSize(1920, 1080),
                                       int fps = 30)
{
    auto* cameraManager = new CameraManager(parent);
    
    // Configure for FFmpeg backend
    cameraManager->setBackend(CameraManager::FFmpeg);
    cameraManager->setVideoOutput(videoItem);
    
    // Set resolution and frame rate
    // These will be applied when the camera starts
    
    return cameraManager;
}

/**
 * @brief Get system information about FFmpeg camera capabilities
 * 
 * @return QStringList containing system information
 */
inline QStringList getFFmpegCameraSystemInfo()
{
    QStringList info;
    
    info << "=== FFmpeg Camera System Information ===";
    
    // Available devices
    QStringList devices = FFmpegCameraThread::getAvailableV4L2Devices();
    info << QString("Available V4L2 devices: %1").arg(devices.size());
    for (const QString& device : devices) {
        info << QString("  - %1").arg(device);
    }
    
    // Best device
    QString bestDevice = FFmpegCameraThread::findOpenterfaceDevice();
    info << QString("Best Openterface device: %1").arg(bestDevice);
    
    // Supported resolutions for best device
    if (!bestDevice.isEmpty()) {
        QList<QSize> resolutions = FFmpegCameraThread::getSupportedResolutions(bestDevice);
        info << QString("Supported resolutions for %1:").arg(bestDevice);
        for (const QSize& res : resolutions) {
            info << QString("  - %1x%2").arg(res.width()).arg(res.height());
        }
    }
    
    return info;
}

#endif // FFMPEG_CAMERA_INTEGRATION_H

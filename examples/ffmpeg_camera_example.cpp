/*
 * Example usage of the FFmpeg Camera Manager for Openterface QT
 * 
 * This file demonstrates how to integrate the new FFmpeg-based camera manager
 * into your existing application to replace the Qt-based camera system.
 */

#include "host/cameramanageradapter.h"
#include "host/ffmpegcameramanager.h"
#include "ui/videopane.h"
#include <QApplication>
#include <QGraphicsVideoItem>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>

class ExampleMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    ExampleMainWindow(QWidget* parent = nullptr) : QMainWindow(parent)
    {
        setupUI();
        setupCamera();
    }

private slots:
    void onCameraActiveChanged(bool active)
    {
        qDebug() << "Camera active state changed:" << active;
    }

    void onCameraError(const QString& error)
    {
        qDebug() << "Camera error:" << error;
    }

    void onFpsChanged(double fps)
    {
        qDebug() << "Current FPS:" << fps;
    }

private:
    void setupUI()
    {
        // Create the central widget and layout
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        QVBoxLayout* layout = new QVBoxLayout(centralWidget);
        
        // Create video pane (from existing Openterface code)
        m_videoPane = new VideoPane(this);
        layout->addWidget(m_videoPane);
        
        setWindowTitle("FFmpeg Camera Manager Example");
        resize(1280, 720);
    }
    
    void setupCamera()
    {
        // Option 1: Use the adapter (recommended for integration)
        m_cameraAdapter = new CameraManagerAdapter(this);
        
        // Set to use FFmpeg backend on Linux
        m_cameraAdapter->setCameraBackend(CameraManagerAdapter::FFmpegBackend);
        
        // Connect signals
        connect(m_cameraAdapter, &CameraManagerAdapter::cameraActiveChanged,
                this, &ExampleMainWindow::onCameraActiveChanged);
        connect(m_cameraAdapter, &CameraManagerAdapter::cameraError,
                this, &ExampleMainWindow::onCameraError);
        connect(m_cameraAdapter, &CameraManagerAdapter::fpsChanged,
                this, &ExampleMainWindow::onFpsChanged);
        
        // Set video output to the video pane's graphics video item
        m_cameraAdapter->setVideoOutput(m_videoPane->getVideoItem());
        
        // Configure camera settings
        m_cameraAdapter->setResolution(QSize(1920, 1080));
        m_cameraAdapter->setFrameRate(30);
        
        // Start the camera
        m_cameraAdapter->startCamera();
        
        /* Option 2: Use FFmpeg camera manager directly
        m_ffmpegCamera = new FFmpegCameraManager(this);
        
        connect(m_ffmpegCamera, &FFmpegCameraManager::cameraActiveChanged,
                this, &ExampleMainWindow::onCameraActiveChanged);
        connect(m_ffmpegCamera, &FFmpegCameraManager::error,
                this, &ExampleMainWindow::onCameraError);
        connect(m_ffmpegCamera, &FFmpegCameraManager::fpsChanged,
                this, &ExampleMainWindow::onFpsChanged);
        
        m_ffmpegCamera->setVideoOutput(m_videoPane->getVideoItem());
        m_ffmpegCamera->setResolution(QSize(1920, 1080));
        m_ffmpegCamera->setFrameRate(30);
        
        // Find and start Openterface camera
        QString openterfaceDevice = m_ffmpegCamera->findOpenterfaceCamera();
        if (!openterfaceDevice.isEmpty()) {
            m_ffmpegCamera->startCamera(openterfaceDevice);
        } else {
            qWarning() << "Openterface camera not found";
        }
        */
    }
    
    VideoPane* m_videoPane;
    CameraManagerAdapter* m_cameraAdapter;
    // FFmpegCameraManager* m_ffmpegCamera;  // Alternative direct usage
};

/*
 * Integration instructions for existing Openterface QT code:
 * 
 * 1. Replace CameraManager usage with CameraManagerAdapter:
 *    - Change: CameraManager* m_cameraManager;
 *    - To:     CameraManagerAdapter* m_cameraManager;
 * 
 * 2. In MainWindow constructor or initialization:
 *    m_cameraManager = new CameraManagerAdapter(this);
 *    m_cameraManager->setCameraBackend(CameraManagerAdapter::FFmpegBackend);
 * 
 * 3. The adapter provides the same interface as CameraManager, so most
 *    existing code should work without changes.
 * 
 * 4. For Linux-specific optimizations, the adapter will automatically
 *    use the FFmpeg backend when shouldUseFFmpegBackend() returns true.
 * 
 * 5. Video output setup remains the same:
 *    m_cameraManager->setVideoOutput(videoPane->getVideoItem());
 * 
 * 6. All existing camera control methods work:
 *    m_cameraManager->startCamera();
 *    m_cameraManager->stopCamera();
 *    m_cameraManager->setResolution(QSize(1920, 1080));
 * 
 * 7. The FFmpeg backend provides additional benefits:
 *    - Multi-threaded decoding for better performance
 *    - Direct V4L2 access for lower latency
 *    - Better support for MJPEG streams from capture cards
 *    - Real-time FPS monitoring
 *    - Automatic device detection for Openterface/MS2109 cards
 */

/*
 * Performance considerations:
 * 
 * 1. Threading: The FFmpeg backend uses a separate thread for decoding,
 *    preventing UI blocking and providing smoother video playback.
 * 
 * 2. Buffer management: Frame buffering prevents dropped frames and
 *    provides consistent video output.
 * 
 * 3. Format conversion: Optimized scaling and format conversion using
 *    FFmpeg's libswscale for best performance.
 * 
 * 4. Memory management: Careful cleanup of FFmpeg resources to prevent
 *    memory leaks.
 * 
 * 5. Device access: Direct V4L2 access bypasses Qt's multimedia layer
 *    for reduced overhead.
 */

/*
 * Configuration options:
 * 
 * Users can control which backend to use via application settings:
 * - "camera/backend" = "auto" (default) - Use best backend for platform
 * - "camera/backend" = "qt" - Force Qt backend
 * - "camera/backend" = "ffmpeg" - Force FFmpeg backend
 * 
 * This allows users to switch backends if they encounter issues or
 * prefer different behavior.
 */

#include "ffmpeg_camera_example.moc"

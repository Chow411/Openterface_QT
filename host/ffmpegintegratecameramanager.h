#ifndef FFMPEGINTEGRATECAMERAMANAGER_H
#define FFMPEGINTEGRATECAMERAMANAGER_H

#include <QObject>
#include <QVideoSink>
#include <QVideoFrame>
#include <QGraphicsVideoItem>
#include <QVideoWidget>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QLoggingCategory>
#include <memory>

#include "ffmpegcamerathread.h"

Q_DECLARE_LOGGING_CATEGORY(log_ffmpeg_integrate)

/**
 * @brief Enhanced camera manager that integrates FFmpeg capture with Qt's multimedia framework
 * 
 * This class provides a bridge between FFmpeg camera capture and Qt's QGraphicsVideoItem/QVideoWidget
 * It handles multi-threaded video capture, frame conversion, and proper display integration.
 */
class FFmpegIntegrateCameraManager : public QObject
{
    Q_OBJECT

public:
    explicit FFmpegIntegrateCameraManager(QObject* parent = nullptr);
    ~FFmpegIntegrateCameraManager();

    // Camera control
    bool startCamera(const QString& devicePath = "");
    void stopCamera();
    bool isActive() const;

    // Configuration
    void setResolution(const QSize& resolution);
    void setFrameRate(int fps);
    QSize resolution() const;
    int frameRate() const;

    // Video output - supports both QGraphicsVideoItem and QVideoWidget
    void setVideoOutput(QGraphicsVideoItem* videoItem);
    void setVideoOutput(QVideoWidget* videoWidget);
    QGraphicsVideoItem* getVideoItem() const { return m_videoItem; }
    QVideoWidget* getVideoWidget() const { return m_videoWidget; }

    // Device discovery and management
    QStringList getAvailableCameras() const;
    QString getCurrentDevice() const { return m_currentDevice; }
    QString findBestCamera() const;

    // Format support
    QList<QSize> getSupportedResolutions() const;
    QList<int> getSupportedFrameRates() const;

    // Performance monitoring
    double getCurrentFPS() const;
    int getDroppedFrames() const;
    QString getPerformanceInfo() const;

    // Frame capture for screenshots
    void takeSnapshot(const QString& filePath = "");
    void takeAreaSnapshot(const QString& filePath, const QRect& area);

public slots:
    void onFrameReady(const QVideoFrame& frame);
    void onCaptureError(const QString& error);
    void onFPSChanged(double fps);

signals:
    void cameraActiveChanged(bool active);
    void frameReady(const QVideoFrame& frame);
    void error(const QString& errorMessage);
    void fpsChanged(double fps);
    void resolutionChanged(const QSize& resolution);
    void snapshotSaved(const QString& filePath);
    void performanceUpdated(const QString& info);

private slots:
    void updatePerformanceInfo();
    void processFrameQueue();

private:
    void initializeVideoSink();
    void cleanupVideoSink();
    void setupFrameProcessor();
    void cleanupFrameProcessor();
    QString generateSnapshotPath() const;
    
    // FFmpeg capture thread
    std::unique_ptr<FFmpegCameraThread> m_captureThread;
    
    // Video output targets
    QGraphicsVideoItem* m_videoItem;
    QVideoWidget* m_videoWidget;
    std::unique_ptr<QVideoSink> m_videoSink;
    
    // Configuration
    QString m_currentDevice;
    QSize m_resolution;
    int m_fps;
    bool m_isActive;
    
    // Frame processing and display
    QTimer* m_frameProcessTimer;
    QQueue<QVideoFrame> m_frameQueue;
    mutable QMutex m_frameQueueMutex;
    static const int MAX_FRAME_QUEUE_SIZE = 5;
    
    // Performance monitoring
    QTimer* m_performanceTimer;
    mutable QMutex m_performanceMutex;
    double m_currentFPS;
    int m_totalFramesProcessed;
    int m_droppedFrames;
    
    // Snapshot functionality
    QString m_snapshotPath;
    QRect m_snapshotArea;
    bool m_takeSnapshotNext;
    bool m_takeAreaSnapshotNext;
};

#endif // FFMPEGINTEGRATECAMERAMANAGER_H

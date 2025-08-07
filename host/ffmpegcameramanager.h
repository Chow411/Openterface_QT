#ifndef FFMPEGCAMERAMANAGER_H
#define FFMPEGCAMERAMANAGER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QQueue>
#include <QVideoFrame>
#include <QVideoSink>
#include <QGraphicsVideoItem>
#include <QVideoWidget>
#include <QSize>
#include <QDebug>
#include <QLoggingCategory>
#include <atomic>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

Q_DECLARE_LOGGING_CATEGORY(log_ffmpeg_camera)

class FFmpegDecodeThread;
class FFmpegCameraManager;

/**
 * @brief Frame buffer for storing decoded video frames
 */
struct VideoFrameBuffer {
    QVideoFrame frame;
    qint64 timestamp;
    
    VideoFrameBuffer() : timestamp(0) {}
    VideoFrameBuffer(const QVideoFrame& f, qint64 ts) : frame(f), timestamp(ts) {}
};

/**
 * @brief Thread class for FFmpeg decoding operations
 */
class FFmpegDecodeThread : public QThread
{
    Q_OBJECT

public:
    explicit FFmpegDecodeThread(FFmpegCameraManager* manager, QObject* parent = nullptr);
    ~FFmpegDecodeThread();

    void startDecoding(const QString& devicePath, const QSize& resolution, int fps);
    void stopDecoding();
    bool isDecoding() const { return m_isDecoding.load(); }
    
    // GPU acceleration methods (public for manager access)
    bool isHardwareAccelerated() const { return m_hwDeviceContext != nullptr; }
    QString getHardwareAccelerationType() const;

signals:
    void frameReady(const QVideoFrame& frame);
    void error(const QString& errorMessage);
    void decodingStarted();
    void decodingStopped();

protected:
    void run() override;

private:
    bool initFFmpeg();
    void cleanupFFmpeg();
    bool openDevice(const QString& devicePath, const QSize& resolution, int fps);
    QVideoFrame convertAVFrameToQVideoFrame(AVFrame* avFrame);
    
    // GPU acceleration methods
    bool initializeHardwareAcceleration();
    void cleanupHardwareAcceleration();
    bool isHardwareFrame(AVFrame* frame) const;
    
    FFmpegCameraManager* m_manager;
    
    // FFmpeg context
    AVFormatContext* m_formatContext;
    AVCodecContext* m_codecContext;
    AVFrame* m_frame;
    AVFrame* m_rgbFrame;
    SwsContext* m_swsContext;
    uint8_t* m_buffer;
    
    // Hardware acceleration
    AVBufferRef* m_hwDeviceContext;
    AVFrame* m_hwFrame;
    enum AVHWDeviceType m_hwDeviceType;
    
    // Stream info
    int m_videoStreamIndex;
    
    // Thread control
    std::atomic<bool> m_isDecoding;
    std::atomic<bool> m_shouldStop;
    
    // Device parameters
    QString m_devicePath;
    QSize m_resolution;
    int m_fps;
    
    mutable QMutex m_mutex;
};

/**
 * @brief FFmpeg-based camera manager for Linux using V4L2 devices
 */
class FFmpegCameraManager : public QObject
{
    Q_OBJECT

public:
    explicit FFmpegCameraManager(QObject* parent = nullptr);
    ~FFmpegCameraManager();

    // Camera control methods
    bool startCamera(const QString& devicePath = "/dev/video0");
    void stopCamera();
    bool isActive() const { return m_isActive; }

    // Configuration methods
    void setResolution(const QSize& resolution);
    void setFrameRate(int fps);
    QSize resolution() const { return m_resolution; }
    int frameRate() const { return m_fps; }

    // Video output methods
    void setVideoOutput(QGraphicsVideoItem* videoItem);
    void setVideoOutput(QVideoWidget* videoWidget);
    QGraphicsVideoItem* getVideoItem() const { return m_videoItem; }

    // Camera discovery
    QStringList getAvailableCameras() const;
    QString findOpenterfaceCamera() const;

    // Format support
    QList<QSize> getSupportedResolutions(const QString& devicePath = "/dev/video0") const;
    QList<int> getSupportedFrameRates(const QString& devicePath = "/dev/video0") const;

    // Performance monitoring
    double getCurrentFPS() const { return m_currentFPS; }
    int getDroppedFrames() const { return m_droppedFrames; }
    
    // Hardware acceleration info
    QString getHardwareAccelerationInfo() const;
    bool isHardwareAccelerated() const;

public slots:
    void onFrameReady(const QVideoFrame& frame);
    void onDecodingError(const QString& error);

signals:
    void cameraActiveChanged(bool active);
    void frameReady(const QVideoFrame& frame);
    void error(const QString& errorMessage);
    void fpsChanged(double fps);
    void resolutionChanged(const QSize& resolution);

private slots:
    void updateFPS();

private:
    void initializeVideoSink();
    void cleanupVideoSink();
    bool queryDeviceCapabilities(const QString& devicePath);
    
    // Thread management
    std::unique_ptr<FFmpegDecodeThread> m_decodeThread;
    
    // Video output
    QGraphicsVideoItem* m_videoItem;
    QVideoWidget* m_videoWidget;
    std::unique_ptr<QVideoSink> m_videoSink;
    
    // Camera configuration
    QString m_currentDevice;
    QSize m_resolution;
    int m_fps;
    bool m_isActive;
    
    // Performance monitoring
    QTimer* m_fpsTimer;
    mutable QMutex m_fpsMutex;
    int m_frameCount;
    double m_currentFPS;
    int m_droppedFrames;
    
    // Frame buffer management
    static const int MAX_BUFFER_SIZE = 5;
    QQueue<VideoFrameBuffer> m_frameBuffer;
    mutable QMutex m_bufferMutex;
};

#endif // FFMPEGCAMERAMANAGER_H

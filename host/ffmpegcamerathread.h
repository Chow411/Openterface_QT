#ifndef FFMPEGCAMERATHREAD_H
#define FFMPEGCAMERATHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QSize>
#include <QQueue>
#include <QTimer>
#include <QElapsedTimer>
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
#include <libavutil/time.h>
}

Q_DECLARE_LOGGING_CATEGORY(log_ffmpeg_thread)

/**
 * @brief Multi-threaded FFmpeg camera capture and decode thread
 * 
 * This class handles camera capture using FFmpeg and V4L2, with frame decoding
 * performed in a separate thread for better performance.
 */
class FFmpegCameraThread : public QThread
{
    Q_OBJECT

public:
    explicit FFmpegCameraThread(QObject* parent = nullptr);
    ~FFmpegCameraThread();

    // Thread control
    bool startCapture(const QString& devicePath, const QSize& resolution = QSize(1920, 1080), int fps = 30);
    void stopCapture();
    bool isCapturing() const { return m_isCapturing.load(); }

    // Configuration
    void setResolution(const QSize& resolution);
    void setFrameRate(int fps);
    QSize resolution() const { return m_resolution; }
    int frameRate() const { return m_fps; }

    // Device discovery
    static QStringList getAvailableV4L2Devices();
    static QString findOpenterfaceDevice();
    static QList<QSize> getSupportedResolutions(const QString& devicePath);

    // Performance monitoring
    double getCurrentFPS() const;
    int getDroppedFrameCount() const { return m_droppedFrames.load(); }
    int getBufferSize() const;

signals:
    void frameReady(const QVideoFrame& frame);
    void captureStarted();
    void captureStopped();
    void error(const QString& errorMessage);
    void fpsChanged(double fps);

protected:
    void run() override;

private:
    // FFmpeg initialization and cleanup
    bool initializeFFmpeg();
    void cleanupFFmpeg();
    
    // Device operations
    bool openDevice();
    void closeDevice();
    bool configureDevice();
    
    // Frame processing
    QVideoFrame convertAVFrameToQVideoFrame(AVFrame* avFrame);
    bool setupScaler();
    void cleanupScaler();
    
    // Buffer management
    void manageFrameBuffer();
    
    // FFmpeg contexts
    AVFormatContext* m_formatContext;
    AVCodecContext* m_codecContext;
    const AVCodec* m_codec;
    AVFrame* m_frame;
    AVFrame* m_rgbFrame;
    SwsContext* m_swsContext;
    uint8_t* m_rgbBuffer;
    
    // Stream information
    int m_videoStreamIndex;
    
    // Device configuration
    QString m_devicePath;
    QSize m_resolution;
    int m_fps;
    
    // Thread control
    std::atomic<bool> m_isCapturing;
    std::atomic<bool> m_shouldStop;
    
    // Performance monitoring
    mutable QMutex m_fpsMutex;
    QElapsedTimer m_fpsTimer;
    int m_frameCount;
    double m_currentFPS;
    std::atomic<int> m_droppedFrames;
    
    // Frame buffer for smoothing
    QQueue<QVideoFrame> m_frameBuffer;
    mutable QMutex m_bufferMutex;
    static const int MAX_BUFFER_SIZE = 3;
    
    // Timing control
    qint64 m_targetFrameInterval; // in microseconds
    QElapsedTimer m_captureTimer;
};

#endif // FFMPEGCAMERATHREAD_H

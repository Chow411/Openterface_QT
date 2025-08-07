#include "ffmpegcamerathread.h"
#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QImage>
#include <QVideoFrameFormat>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <glob.h>

Q_LOGGING_CATEGORY(log_ffmpeg_thread, "opf.ffmpeg.thread")

FFmpegCameraThread::FFmpegCameraThread(QObject* parent)
    : QThread(parent)
    , m_formatContext(nullptr)
    , m_codecContext(nullptr)
    , m_codec(nullptr)
    , m_frame(nullptr)
    , m_rgbFrame(nullptr)
    , m_swsContext(nullptr)
    , m_rgbBuffer(nullptr)
    , m_videoStreamIndex(-1)
    , m_resolution(1920, 1080)
    , m_fps(30)
    , m_isCapturing(false)
    , m_shouldStop(false)
    , m_frameCount(0)
    , m_currentFPS(0.0)
    , m_droppedFrames(0)
    , m_targetFrameInterval(33333) // ~30 FPS in microseconds
{
    // Initialize FFmpeg logging
    av_log_set_level(AV_LOG_WARNING);
    
    qCDebug(log_ffmpeg_thread) << "FFmpegCameraThread created";
}

FFmpegCameraThread::~FFmpegCameraThread()
{
    stopCapture();
    cleanupFFmpeg();
    qCDebug(log_ffmpeg_thread) << "FFmpegCameraThread destroyed";
}

bool FFmpegCameraThread::startCapture(const QString& devicePath, const QSize& resolution, int fps)
{
    if (m_isCapturing.load()) {
        qCWarning(log_ffmpeg_thread) << "Already capturing, stopping current session";
        stopCapture();
    }
    
    m_devicePath = devicePath;
    m_resolution = resolution;
    m_fps = fps;
    m_targetFrameInterval = 1000000 / fps; // Convert to microseconds
    m_shouldStop.store(false);
    m_droppedFrames.store(0);
    
    qCDebug(log_ffmpeg_thread) << "Starting capture for device:" << devicePath 
                               << "resolution:" << resolution << "fps:" << fps;
    
    // Start the thread
    start();
    
    // Wait a moment to see if initialization succeeded
    if (!wait(100)) {
        // Thread is still starting, assume success for now
        return true;
    }
    
    return m_isCapturing.load();
}

void FFmpegCameraThread::stopCapture()
{
    qCDebug(log_ffmpeg_thread) << "Stopping capture";
    
    m_shouldStop.store(true);
    
    if (isRunning()) {
        if (!wait(3000)) {  // Wait up to 3 seconds
            qCWarning(log_ffmpeg_thread) << "Thread failed to stop gracefully, terminating";
            terminate();
            wait(1000);
        }
    }
    
    m_isCapturing.store(false);
    emit captureStopped();
}

void FFmpegCameraThread::setResolution(const QSize& resolution)
{
    QMutexLocker locker(&m_bufferMutex);
    m_resolution = resolution;
}

void FFmpegCameraThread::setFrameRate(int fps)
{
    QMutexLocker locker(&m_fpsMutex);
    m_fps = fps;
    m_targetFrameInterval = 1000000 / fps;
}

double FFmpegCameraThread::getCurrentFPS() const
{
    QMutexLocker locker(&m_fpsMutex);
    return m_currentFPS;
}

int FFmpegCameraThread::getBufferSize() const
{
    QMutexLocker locker(&m_bufferMutex);
    return m_frameBuffer.size();
}

void FFmpegCameraThread::run()
{
    qCDebug(log_ffmpeg_thread) << "Camera thread starting";
    
    if (!initializeFFmpeg()) {
        emit error("Failed to initialize FFmpeg");
        return;
    }
    
    if (!openDevice()) {
        emit error(QString("Failed to open device: %1").arg(m_devicePath));
        cleanupFFmpeg();
        return;
    }
    
    if (!configureDevice()) {
        emit error("Failed to configure device");
        closeDevice();
        cleanupFFmpeg();
        return;
    }
    
    if (!setupScaler()) {
        emit error("Failed to setup frame scaler");
        closeDevice();
        cleanupFFmpeg();
        return;
    }
    
    m_isCapturing.store(true);
    emit captureStarted();
    
    m_fpsTimer.start();
    m_captureTimer.start();
    
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        emit error("Failed to allocate packet");
        closeDevice();
        cleanupFFmpeg();
        return;
    }
    
    qCDebug(log_ffmpeg_thread) << "Starting capture loop";
    
    while (!m_shouldStop.load()) {
        // Throttle frame capture to target FPS
        qint64 elapsed = m_captureTimer.nsecsElapsed() / 1000; // Convert to microseconds
        if (elapsed < m_targetFrameInterval) {
            msleep(1);
            continue;
        }
        m_captureTimer.restart();
        
        int ret = av_read_frame(m_formatContext, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                qCDebug(log_ffmpeg_thread) << "End of stream reached";
                break;
            } else if (ret == AVERROR(EAGAIN)) {
                msleep(1);
                continue;
            } else {
                char error_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                qCWarning(log_ffmpeg_thread) << "Error reading frame:" << error_buf;
                // Continue trying to read frames
                msleep(10);
                continue;
            }
        }
        
        if (packet->stream_index == m_videoStreamIndex) {
            ret = avcodec_send_packet(m_codecContext, packet);
            if (ret < 0) {
                char error_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                qCWarning(log_ffmpeg_thread) << "Error sending packet:" << error_buf;
                av_packet_unref(packet);
                continue;
            }
            
            while (ret >= 0 && !m_shouldStop.load()) {
                ret = avcodec_receive_frame(m_codecContext, m_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    char error_buf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                    qCWarning(log_ffmpeg_thread) << "Error receiving frame:" << error_buf;
                    break;
                }
                
                // Convert frame to QVideoFrame
                QVideoFrame videoFrame = convertAVFrameToQVideoFrame(m_frame);
                if (videoFrame.isValid()) {
                    // Manage frame buffer to prevent memory buildup
                    manageFrameBuffer();
                    
                    {
                        QMutexLocker locker(&m_bufferMutex);
                        m_frameBuffer.enqueue(videoFrame);
                    }
                    
                    emit frameReady(videoFrame);
                    
                    // Update FPS counter
                    m_frameCount++;
                    if (m_fpsTimer.elapsed() >= 1000) { // Update every second
                        QMutexLocker locker(&m_fpsMutex);
                        m_currentFPS = m_frameCount * 1000.0 / m_fpsTimer.elapsed();
                        emit fpsChanged(m_currentFPS);
                        m_frameCount = 0;
                        m_fpsTimer.restart();
                    }
                } else {
                    m_droppedFrames.fetch_add(1);
                }
            }
        }
        
        av_packet_unref(packet);
    }
    
    av_packet_free(&packet);
    closeDevice();
    cleanupFFmpeg();
    
    m_isCapturing.store(false);
    emit captureStopped();
    
    qCDebug(log_ffmpeg_thread) << "Camera thread finished";
}

bool FFmpegCameraThread::initializeFFmpeg()
{
    qCDebug(log_ffmpeg_thread) << "Initializing FFmpeg";
    
    // Allocate frames
    m_frame = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    
    if (!m_frame || !m_rgbFrame) {
        qCCritical(log_ffmpeg_thread) << "Failed to allocate frames";
        return false;
    }
    
    qCDebug(log_ffmpeg_thread) << "FFmpeg initialized successfully";
    return true;
}

void FFmpegCameraThread::cleanupFFmpeg()
{
    qCDebug(log_ffmpeg_thread) << "Cleaning up FFmpeg resources";
    
    cleanupScaler();
    
    if (m_rgbBuffer) {
        av_freep(&m_rgbBuffer);
        m_rgbBuffer = nullptr;
    }
    
    if (m_rgbFrame) {
        av_frame_free(&m_rgbFrame);
        m_rgbFrame = nullptr;
    }
    
    if (m_frame) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }
}

bool FFmpegCameraThread::openDevice()
{
    qCDebug(log_ffmpeg_thread) << "Opening device:" << m_devicePath;
    
    // Find V4L2 input format
    const AVInputFormat* inputFormat = av_find_input_format("v4l2");
    if (!inputFormat) {
        qCCritical(log_ffmpeg_thread) << "V4L2 input format not found";
        return false;
    }
    
    // Allocate format context
    m_formatContext = avformat_alloc_context();
    if (!m_formatContext) {
        qCCritical(log_ffmpeg_thread) << "Failed to allocate format context";
        return false;
    }
    
    // Set device options
    AVDictionary* options = nullptr;
    av_dict_set(&options, "video_size", QString("%1x%2").arg(m_resolution.width()).arg(m_resolution.height()).toUtf8().constData(), 0);
    av_dict_set(&options, "framerate", QString::number(m_fps).toUtf8().constData(), 0);
    av_dict_set(&options, "pixel_format", "yuyv422", 0); // Common format for USB cameras
    
    // Open input
    int ret = avformat_open_input(&m_formatContext, m_devicePath.toUtf8().constData(), inputFormat, &options);
    av_dict_free(&options);
    
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        qCCritical(log_ffmpeg_thread) << "Failed to open input:" << error_buf;
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
        return false;
    }
    
    // Find stream information
    ret = avformat_find_stream_info(m_formatContext, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        qCWarning(log_ffmpeg_thread) << "Failed to find stream info:" << error_buf;
    }
    
    // Find video stream
    const AVCodec* codec = nullptr;
    m_videoStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (m_videoStreamIndex < 0) {
        qCCritical(log_ffmpeg_thread) << "No video stream found";
        return false;
    }
    
    m_codec = codec; // Store the codec
    
    qCDebug(log_ffmpeg_thread) << "Found video stream at index:" << m_videoStreamIndex;
    return true;
}

void FFmpegCameraThread::closeDevice()
{
    qCDebug(log_ffmpeg_thread) << "Closing device";
    
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
    }
    
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
        m_formatContext = nullptr;
    }
}

bool FFmpegCameraThread::configureDevice()
{
    qCDebug(log_ffmpeg_thread) << "Configuring device";
    
    if (!m_codec) {
        qCCritical(log_ffmpeg_thread) << "No codec found";
        return false;
    }
    
    // Allocate codec context
    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext) {
        qCCritical(log_ffmpeg_thread) << "Failed to allocate codec context";
        return false;
    }
    
    // Copy parameters from stream
    AVStream* stream = m_formatContext->streams[m_videoStreamIndex];
    int ret = avcodec_parameters_to_context(m_codecContext, stream->codecpar);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        qCCritical(log_ffmpeg_thread) << "Failed to copy codec parameters:" << error_buf;
        return false;
    }
    
    // Open codec
    ret = avcodec_open2(m_codecContext, m_codec, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        qCCritical(log_ffmpeg_thread) << "Failed to open codec:" << error_buf;
        return false;
    }
    
    qCDebug(log_ffmpeg_thread) << "Device configured successfully"
                               << "codec:" << m_codec->name
                               << "size:" << m_codecContext->width << "x" << m_codecContext->height;
    
    return true;
}

bool FFmpegCameraThread::setupScaler()
{
    qCDebug(log_ffmpeg_thread) << "Setting up frame scaler";
    
    if (!m_codecContext) {
        qCCritical(log_ffmpeg_thread) << "Codec context not available";
        return false;
    }
    
    // Setup scaler context
    m_swsContext = sws_getContext(
        m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
        m_resolution.width(), m_resolution.height(), AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!m_swsContext) {
        qCCritical(log_ffmpeg_thread) << "Failed to create scaler context";
        return false;
    }
    
    // Allocate RGB buffer
    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_resolution.width(), m_resolution.height(), 1);
    m_rgbBuffer = (uint8_t*)av_malloc(bufferSize);
    if (!m_rgbBuffer) {
        qCCritical(log_ffmpeg_thread) << "Failed to allocate RGB buffer";
        return false;
    }
    
    // Setup RGB frame
    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize, m_rgbBuffer,
                        AV_PIX_FMT_RGB24, m_resolution.width(), m_resolution.height(), 1);
    
    qCDebug(log_ffmpeg_thread) << "Scaler setup complete";
    return true;
}

void FFmpegCameraThread::cleanupScaler()
{
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
}

QVideoFrame FFmpegCameraThread::convertAVFrameToQVideoFrame(AVFrame* avFrame)
{
    if (!avFrame || !m_swsContext || !m_rgbFrame) {
        return QVideoFrame();
    }
    
    // Scale frame to RGB
    int ret = sws_scale(m_swsContext,
                       avFrame->data, avFrame->linesize, 0, avFrame->height,
                       m_rgbFrame->data, m_rgbFrame->linesize);
    
    if (ret <= 0) {
        qCWarning(log_ffmpeg_thread) << "Failed to scale frame";
        return QVideoFrame();
    }
    
    // Create QImage from RGB data
    QImage image(m_rgbFrame->data[0], m_resolution.width(), m_resolution.height(), 
                m_rgbFrame->linesize[0], QImage::Format_RGB888);
    
    if (image.isNull()) {
        qCWarning(log_ffmpeg_thread) << "Failed to create QImage";
        return QVideoFrame();
    }
    
    // Convert to QVideoFrame  
    // In Qt6, need to create with proper format and copy image data
    QVideoFrameFormat format(m_resolution, QVideoFrameFormat::Format_RGBX8888);
    QVideoFrame videoFrame(format);
    
    if (videoFrame.map(QVideoFrame::WriteOnly)) {
        // Copy image data to video frame
        const QImage rgbImage = image.convertToFormat(QImage::Format_RGBX8888);
        memcpy(videoFrame.bits(0), rgbImage.constBits(), rgbImage.sizeInBytes());
        videoFrame.unmap();
    }
    
    videoFrame.setStartTime(av_gettime());
    
    return videoFrame;
}

void FFmpegCameraThread::manageFrameBuffer()
{
    QMutexLocker locker(&m_bufferMutex);
    
    // Keep buffer size under control
    while (m_frameBuffer.size() >= MAX_BUFFER_SIZE) {
        m_frameBuffer.dequeue();
        m_droppedFrames.fetch_add(1);
    }
}

QStringList FFmpegCameraThread::getAvailableV4L2Devices()
{
    QStringList devices;
    
    // Use glob to find video devices
    glob_t globbuf;
    if (glob("/dev/video*", 0, nullptr, &globbuf) == 0) {
        for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
            QString devicePath = QString::fromUtf8(globbuf.gl_pathv[i]);
            
            // Test if device can be opened
            int fd = open(devicePath.toUtf8().constData(), O_RDWR);
            if (fd >= 0) {
                // Check if it's a capture device
                struct v4l2_capability cap;
                if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
                    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
                        devices.append(devicePath);
                        qCDebug(log_ffmpeg_thread) << "Found V4L2 device:" << devicePath << QString::fromUtf8((char*)cap.card);
                    }
                }
                close(fd);
            }
        }
        globfree(&globbuf);
    }
    
    return devices;
}

QString FFmpegCameraThread::findOpenterfaceDevice()
{
    QStringList devices = getAvailableV4L2Devices();
    
    for (const QString& device : devices) {
        int fd = open(device.toUtf8().constData(), O_RDWR);
        if (fd >= 0) {
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
                QString cardName = QString::fromUtf8((char*)cap.card).toLower();
                // Look for common Openterface device names
                if (cardName.contains("openterface") || 
                    cardName.contains("ms2109") || 
                    cardName.contains("usb video") ||
                    cardName.contains("capture")) {
                    close(fd);
                    qCDebug(log_ffmpeg_thread) << "Found potential Openterface device:" << device << cardName;
                    return device;
                }
            }
            close(fd);
        }
    }
    
    // Return first device as fallback
    return devices.isEmpty() ? "/dev/video0" : devices.first();
}

QList<QSize> FFmpegCameraThread::getSupportedResolutions(const QString& devicePath)
{
    QList<QSize> resolutions;
    
    int fd = open(devicePath.toUtf8().constData(), O_RDWR);
    if (fd < 0) {
        qCWarning(log_ffmpeg_thread) << "Failed to open device for resolution query:" << devicePath;
        return resolutions;
    }
    
    struct v4l2_frmsizeenum frmsize;
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.pixel_format = V4L2_PIX_FMT_YUYV; // Common format
    
    for (frmsize.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0; frmsize.index++) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            QSize resolution(frmsize.discrete.width, frmsize.discrete.height);
            if (!resolutions.contains(resolution)) {
                resolutions.append(resolution);
            }
        }
    }
    
    close(fd);
    
    // Add common resolutions if none found
    if (resolutions.isEmpty()) {
        resolutions << QSize(640, 480) << QSize(1280, 720) << QSize(1920, 1080);
    }
    
    qCDebug(log_ffmpeg_thread) << "Supported resolutions for" << devicePath << ":" << resolutions;
    return resolutions;
}

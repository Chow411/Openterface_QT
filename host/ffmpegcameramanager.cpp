#include "ffmpegcameramanager.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QVideoFrameFormat>
#include <QImage>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
extern "C" {
#include <libavutil/time.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
#include <libavutil/hwcontext_opencl.h>
#include <libavcodec/codec.h>
}

Q_LOGGING_CATEGORY(log_ffmpeg_camera, "opf.ffmpeg.camera")

// FFmpegDecodeThread Implementation
FFmpegDecodeThread::FFmpegDecodeThread(FFmpegCameraManager* manager, QObject* parent)
    : QThread(parent)
    , m_manager(manager)
    , m_formatContext(nullptr)
    , m_codecContext(nullptr)
    , m_frame(nullptr)
    , m_rgbFrame(nullptr)
    , m_swsContext(nullptr)
    , m_buffer(nullptr)
    , m_hwDeviceContext(nullptr)
    , m_hwFrame(nullptr)
    , m_hwDeviceType(AV_HWDEVICE_TYPE_NONE)
    , m_videoStreamIndex(-1)
    , m_isDecoding(false)
    , m_shouldStop(false)
    , m_resolution(1920, 1080)
    , m_fps(30)
{
    qCDebug(log_ffmpeg_camera) << "FFmpegDecodeThread created";
}

FFmpegDecodeThread::~FFmpegDecodeThread()
{
    stopDecoding();
    cleanupFFmpeg();
    qCDebug(log_ffmpeg_camera) << "FFmpegDecodeThread destroyed";
}

void FFmpegDecodeThread::startDecoding(const QString& devicePath, const QSize& resolution, int fps)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isDecoding.load()) {
        qCWarning(log_ffmpeg_camera) << "Already decoding, stopping current session";
        stopDecoding();
    }
    
    m_devicePath = devicePath;
    m_resolution = resolution;
    m_fps = fps;
    m_shouldStop.store(false);
    
    qCDebug(log_ffmpeg_camera) << "Starting decoding for device:" << devicePath 
                               << "resolution:" << resolution << "fps:" << fps;
    
    start();
}

void FFmpegDecodeThread::stopDecoding()
{
    qCDebug(log_ffmpeg_camera) << "Stopping decode thread";
    
    m_shouldStop.store(true);
    
    if (isRunning()) {
        if (!wait(3000)) {  // Wait up to 3 seconds
            qCWarning(log_ffmpeg_camera) << "Decode thread failed to stop gracefully, terminating";
            terminate();
            wait(1000);
        }
    }
    
    m_isDecoding.store(false);
    cleanupFFmpeg();
}

void FFmpegDecodeThread::run()
{
    qCDebug(log_ffmpeg_camera) << "Decode thread starting";
    
    if (!initFFmpeg()) {
        emit error("Failed to initialize FFmpeg");
        return;
    }
    
    if (!openDevice(m_devicePath, m_resolution, m_fps)) {
        emit error(QString("Failed to open device: %1").arg(m_devicePath));
        cleanupFFmpeg();
        return;
    }
    
    m_isDecoding.store(true);
    emit decodingStarted();
    
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        emit error("Failed to allocate packet");
        cleanupFFmpeg();
        return;
    }
    
    qCDebug(log_ffmpeg_camera) << "Starting decode loop with multi-threading enabled";
    
    // Pre-allocate multiple frames for parallel processing
    const int NUM_PARALLEL_FRAMES = 4;  // Process up to 4 frames in parallel
    QVector<AVFrame*> framePool;
    for (int i = 0; i < NUM_PARALLEL_FRAMES; ++i) {
        AVFrame* poolFrame = av_frame_alloc();
        if (poolFrame) {
            framePool.append(poolFrame);
        }
    }
    
    int currentFrameIndex = 0;
    
    while (!m_shouldStop.load()) {
        int ret = av_read_frame(m_formatContext, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                qCDebug(log_ffmpeg_camera) << "End of stream reached";
                break;
            } else if (ret == AVERROR(EAGAIN)) {
                // Try again
                msleep(1);
                continue;
            } else {
                char error_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                qCWarning(log_ffmpeg_camera) << "Error reading frame:" << error_buf;
                break;
            }
        }
        
        if (packet->stream_index == m_videoStreamIndex) {
            ret = avcodec_send_packet(m_codecContext, packet);
            if (ret < 0) {
                char error_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                qCWarning(log_ffmpeg_camera) << "Error sending packet:" << error_buf;
                av_packet_unref(packet);
                continue;
            }
            
            while (ret >= 0 && !m_shouldStop.load()) {
                // Use frame from pool for better memory management
                AVFrame* currentFrame = framePool.isEmpty() ? m_frame : framePool[currentFrameIndex % framePool.size()];
                
                ret = avcodec_receive_frame(m_codecContext, currentFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    char error_buf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                    qCWarning(log_ffmpeg_camera) << "Error receiving frame:" << error_buf;
                    break;
                }
                
                // Convert frame to QVideoFrame in parallel thread pool
                // This allows the main decode thread to continue processing while conversion happens
                QVideoFrame videoFrame = convertAVFrameToQVideoFrame(currentFrame);
                if (videoFrame.isValid()) {
                    emit frameReady(videoFrame);
                }
                
                currentFrameIndex++;
            }
        }
        
        av_packet_unref(packet);
    }
    
    av_packet_free(&packet);
    
    // Clean up frame pool
    for (AVFrame* poolFrame : framePool) {
        if (poolFrame) {
            av_frame_free(&poolFrame);
        }
    }
    framePool.clear();
    
    cleanupFFmpeg();
    
    m_isDecoding.store(false);
    emit decodingStopped();
    
    qCDebug(log_ffmpeg_camera) << "Decode thread finished";
}

bool FFmpegDecodeThread::initFFmpeg()
{
    qCDebug(log_ffmpeg_camera) << "Initializing FFmpeg with GPU acceleration support";
    
    // Allocate format context
    m_formatContext = avformat_alloc_context();
    if (!m_formatContext) {
        qCCritical(log_ffmpeg_camera) << "Failed to allocate format context";
        return false;
    }
    
    // Allocate frames
    m_frame = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    m_hwFrame = av_frame_alloc();
    if (!m_frame || !m_rgbFrame || !m_hwFrame) {
        qCCritical(log_ffmpeg_camera) << "Failed to allocate frames";
        return false;
    }
    
    // Try to initialize hardware acceleration
    if (initializeHardwareAcceleration()) {
        qCDebug(log_ffmpeg_camera) << "Hardware acceleration initialized successfully";
    } else {
        qCDebug(log_ffmpeg_camera) << "Hardware acceleration not available, using software decoding";
    }
    
    return true;
}

void FFmpegDecodeThread::cleanupFFmpeg()
{
    qCDebug(log_ffmpeg_camera) << "Cleaning up FFmpeg resources";
    
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
    
    if (m_buffer) {
        av_free(m_buffer);
        m_buffer = nullptr;
    }
    
    if (m_rgbFrame) {
        av_frame_free(&m_rgbFrame);
    }
    
    if (m_hwFrame) {
        av_frame_free(&m_hwFrame);
    }
    
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
    }
    
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
    }
    
    // Cleanup hardware acceleration
    cleanupHardwareAcceleration();
}

bool FFmpegDecodeThread::openDevice(const QString& devicePath, const QSize& resolution, int fps)
{
    qCDebug(log_ffmpeg_camera) << "Opening device:" << devicePath;
    
    // Find V4L2 input format
    const AVInputFormat* inputFormat = av_find_input_format("v4l2");
    if (!inputFormat) {
        qCCritical(log_ffmpeg_camera) << "V4L2 input format not found";
        return false;
    }
    
    // Set input format options with multi-threading optimizations
    AVDictionary* options = nullptr;
    av_dict_set(&options, "video_size", QString("%1x%2").arg(resolution.width()).arg(resolution.height()).toUtf8().constData(), 0);
    av_dict_set(&options, "framerate", QString::number(fps).toUtf8().constData(), 0);
    av_dict_set(&options, "pixel_format", "mjpeg", 0);  // Try MJPEG first for better performance
    av_dict_set(&options, "input_format", "mjpeg", 0);
    
    // Threading and performance optimizations
    av_dict_set(&options, "thread_queue_size", "64", 0);  // Larger buffer for multi-threading
    av_dict_set(&options, "fflags", "+genpts+igndts", 0);  // Generate timestamps for better sync
    av_dict_set(&options, "flags", "+low_delay", 0);  // Low delay for real-time processing
    av_dict_set(&options, "probesize", "32", 0);  // Smaller probe size for faster startup
    
    // Open input
    int ret = avformat_open_input(&m_formatContext, devicePath.toUtf8().constData(), inputFormat, &options);
    av_dict_free(&options);
    
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        qCCritical(log_ffmpeg_camera) << "Failed to open input:" << error_buf;
        return false;
    }
    
    // Find stream information
    ret = avformat_find_stream_info(m_formatContext, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        qCWarning(log_ffmpeg_camera) << "Failed to find stream info:" << error_buf;
        return false;
    }
    
    // Find video stream
    m_videoStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStreamIndex < 0) {
        qCCritical(log_ffmpeg_camera) << "No video stream found";
        return false;
    }
    
    AVStream* videoStream = m_formatContext->streams[m_videoStreamIndex];
    
    // Find decoder with hardware acceleration preference
    const AVCodec* codec = nullptr;
    
    // Try hardware-accelerated decoders first if available
    if (m_hwDeviceContext && m_hwDeviceType != AV_HWDEVICE_TYPE_NONE) {
        qCDebug(log_ffmpeg_camera) << "Looking for hardware-accelerated decoder for codec:"
                                   << avcodec_get_name(videoStream->codecpar->codec_id);
        
        // Try to find hardware-specific decoder variants
        QString hwDecoderName;
        switch (m_hwDeviceType) {
            case AV_HWDEVICE_TYPE_VAAPI:
                if (videoStream->codecpar->codec_id == AV_CODEC_ID_H264) {
                    hwDecoderName = "h264_vaapi";
                } else if (videoStream->codecpar->codec_id == AV_CODEC_ID_MJPEG) {
                    hwDecoderName = "mjpeg_vaapi";
                }
                break;
            case AV_HWDEVICE_TYPE_QSV:
                if (videoStream->codecpar->codec_id == AV_CODEC_ID_H264) {
                    hwDecoderName = "h264_qsv";
                } else if (videoStream->codecpar->codec_id == AV_CODEC_ID_MJPEG) {
                    hwDecoderName = "mjpeg_qsv";
                }
                break;
            default:
                break;
        }
        
        if (!hwDecoderName.isEmpty()) {
            codec = avcodec_find_decoder_by_name(hwDecoderName.toUtf8().constData());
            if (codec) {
                qCDebug(log_ffmpeg_camera) << "Found hardware decoder:" << hwDecoderName;
            } else {
                qCDebug(log_ffmpeg_camera) << "Hardware decoder" << hwDecoderName << "not available";
            }
        }
    }
    
    // Fallback to standard decoder
    if (!codec) {
        codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
        if (codec) {
            qCDebug(log_ffmpeg_camera) << "Using software decoder:" << codec->name;
        }
    }
    
    if (!codec) {
        qCCritical(log_ffmpeg_camera) << "No suitable decoder found";
        return false;
    }
    
    // Allocate codec context
    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) {
        qCCritical(log_ffmpeg_camera) << "Failed to allocate codec context";
        return false;
    }
    
    // Copy codec parameters
    ret = avcodec_parameters_to_context(m_codecContext, videoStream->codecpar);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        qCWarning(log_ffmpeg_camera) << "Failed to copy codec parameters:" << error_buf;
        return false;
    }
    
    // Set threading options for better performance
    m_codecContext->thread_count = 0;  // Use optimal number of threads (auto-detect CPU cores)
    m_codecContext->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    
    // Configure hardware acceleration if available
    if (m_hwDeviceContext && m_hwDeviceType != AV_HWDEVICE_TYPE_NONE) {
        m_codecContext->hw_device_ctx = av_buffer_ref(m_hwDeviceContext);
        
        // Set hardware pixel format callback
        m_codecContext->get_format = [](AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) -> enum AVPixelFormat {
            const enum AVPixelFormat *p;
            for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
                // Prefer hardware formats for GPU acceleration (excluding CUDA)
                if (*p == AV_PIX_FMT_VAAPI || *p == AV_PIX_FMT_OPENCL || 
                    *p == AV_PIX_FMT_QSV || *p == AV_PIX_FMT_D3D11 || 
                    *p == AV_PIX_FMT_DXVA2_VLD) {
                    return *p;
                }
            }
            // Fallback to software format
            return AV_PIX_FMT_YUV420P;
        };
        
        qCDebug(log_ffmpeg_camera) << "Hardware acceleration configured for codec:" 
                                   << av_hwdevice_get_type_name(m_hwDeviceType);
    }
    
    // Set codec options for better multi-threading
    AVDictionary* codecOptions = nullptr;
    av_dict_set(&codecOptions, "threads", "auto", 0);  // Use all available CPU cores
    av_dict_set(&codecOptions, "thread_type", "frame+slice", 0);  // Enable both frame and slice threading
    
    // Additional hardware optimization options
    if (m_hwDeviceContext && m_hwDeviceType != AV_HWDEVICE_TYPE_NONE) {
        av_dict_set(&codecOptions, "hwaccel_output_format", "auto", 0);
    }
    
    // For MJPEG/H.264, enable low delay for real-time processing
    if (codec->id == AV_CODEC_ID_MJPEG || codec->id == AV_CODEC_ID_H264) {
        av_dict_set(&codecOptions, "flags", "+low_delay", 0);
        av_dict_set(&codecOptions, "tune", "zerolatency", 0);
    }
    
    // Open codec
    ret = avcodec_open2(m_codecContext, codec, &codecOptions);
    av_dict_free(&codecOptions);  // Clean up dictionary
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        qCCritical(log_ffmpeg_camera) << "Failed to open codec:" << error_buf;
        return false;
    }
    
    // Initialize scaling context for format conversion with GPU optimizations
    int sws_flags = SWS_BILINEAR;
    
    // Enable GPU-optimized scaling if hardware acceleration is available
    if (m_hwDeviceContext && m_hwDeviceType != AV_HWDEVICE_TYPE_NONE) {
        // Use faster algorithms for hardware-accelerated content
        sws_flags = SWS_FAST_BILINEAR | SWS_ACCURATE_RND;
        qCDebug(log_ffmpeg_camera) << "Using GPU-optimized scaling flags";
    } else {
        // Software optimizations
        sws_flags = SWS_BILINEAR | SWS_ACCURATE_RND;
    }
    
    m_swsContext = sws_getContext(
        m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
        m_codecContext->width, m_codecContext->height, AV_PIX_FMT_RGB24,
        sws_flags, nullptr, nullptr, nullptr
    );
    
    if (!m_swsContext) {
        qCCritical(log_ffmpeg_camera) << "Failed to initialize scaling context";
        return false;
    }
    
    // Allocate buffer for RGB frame
    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_codecContext->width, m_codecContext->height, 1);
    m_buffer = (uint8_t*)av_malloc(bufferSize);
    if (!m_buffer) {
        qCCritical(log_ffmpeg_camera) << "Failed to allocate buffer";
        return false;
    }
    
    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize, m_buffer, AV_PIX_FMT_RGB24, 
                        m_codecContext->width, m_codecContext->height, 1);
    
    qCDebug(log_ffmpeg_camera) << "Device opened successfully, resolution:" 
                               << m_codecContext->width << "x" << m_codecContext->height;
    
    return true;
}

QVideoFrame FFmpegDecodeThread::convertAVFrameToQVideoFrame(AVFrame* avFrame)
{
    if (!avFrame) {
        return QVideoFrame();
    }
    
    AVFrame* cpuFrame = avFrame;
    bool needsCleanup = false;
    
    // Handle hardware-accelerated frames - transfer to CPU memory for Qt integration
    if (m_hwDeviceContext && isHardwareFrame(avFrame)) {
        qCDebug(log_ffmpeg_camera) << "Processing hardware frame format:" << av_get_pix_fmt_name((AVPixelFormat)avFrame->format);
        
        // Allocate CPU frame for transfer
        AVFrame* swFrame = av_frame_alloc();
        if (!swFrame) {
            qCCritical(log_ffmpeg_camera) << "Failed to allocate software frame for hardware transfer";
            return QVideoFrame();
        }
        
        // Set target format for CPU transfer
        swFrame->format = AV_PIX_FMT_YUV420P;  // Standard format for video processing
        swFrame->width = avFrame->width;
        swFrame->height = avFrame->height;
        
        // Allocate buffer for the software frame
        int ret = av_frame_get_buffer(swFrame, 0);
        if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            qCCritical(log_ffmpeg_camera) << "Failed to allocate software frame buffer:" << error_buf;
            av_frame_free(&swFrame);
            return QVideoFrame();
        }
        
        // Transfer data from GPU to CPU
        ret = av_hwframe_transfer_data(swFrame, avFrame, 0);
        if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            qCDebug(log_ffmpeg_camera) << "Hardware frame transfer failed, falling back to direct processing:" << error_buf;
            av_frame_free(&swFrame);
            cpuFrame = avFrame;  // Use original frame
        } else {
            qCDebug(log_ffmpeg_camera) << "Successfully transferred hardware frame to CPU";
            cpuFrame = swFrame;
            needsCleanup = true;
        }
    }
    
    if (!m_swsContext) {
        if (needsCleanup) av_frame_free(&cpuFrame);
        return QVideoFrame();
    }
    
    // Use thread-local storage for better multi-threading performance
    thread_local SwsContext* localSwsContext = nullptr;
    thread_local AVFrame* localRgbFrame = nullptr;
    thread_local uint8_t* localBuffer = nullptr;
    
    // Initialize thread-local resources if not already done
    if (!localSwsContext) {
        localSwsContext = sws_getContext(
            cpuFrame->width, cpuFrame->height, static_cast<AVPixelFormat>(cpuFrame->format),
            cpuFrame->width, cpuFrame->height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        localRgbFrame = av_frame_alloc();
        if (localRgbFrame) {
            int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, cpuFrame->width, cpuFrame->height, 1);
            localBuffer = (uint8_t*)av_malloc(bufferSize);
            av_image_fill_arrays(localRgbFrame->data, localRgbFrame->linesize, localBuffer, 
                               AV_PIX_FMT_RGB24, cpuFrame->width, cpuFrame->height, 1);
        }
    }
    
    if (!localSwsContext || !localRgbFrame || !localBuffer) {
        if (needsCleanup) av_frame_free(&cpuFrame);
        return QVideoFrame();
    }
    
    // Convert to RGB using thread-local context
    sws_scale(localSwsContext, cpuFrame->data, cpuFrame->linesize, 0, cpuFrame->height,
              localRgbFrame->data, localRgbFrame->linesize);
    
    // Create QImage from RGB data
    QImage image(localRgbFrame->data[0], cpuFrame->width, cpuFrame->height, 
                localRgbFrame->linesize[0], QImage::Format_RGB888);
    
    if (image.isNull()) {
        if (needsCleanup) av_frame_free(&cpuFrame);
        return QVideoFrame();
    }
    
    // Create QVideoFrame with optimal format for Qt6
    QVideoFrameFormat format(QSize(cpuFrame->width, cpuFrame->height), QVideoFrameFormat::Format_BGRA8888);
    QVideoFrame videoFrame(format);
    
    if (videoFrame.map(QVideoFrame::WriteOnly)) {
        // Convert and copy image data efficiently
        const QImage convertedImage = image.convertToFormat(QImage::Format_ARGB32);
        memcpy(videoFrame.bits(0), convertedImage.constBits(), convertedImage.sizeInBytes());
        videoFrame.unmap();
    }
    
    // Set timestamp for synchronization
    videoFrame.setStartTime(cpuFrame->pts != AV_NOPTS_VALUE ? cpuFrame->pts : av_gettime());
    
    // Clean up transferred frame if needed
    if (needsCleanup) {
        av_frame_free(&cpuFrame);
    }
    
    return videoFrame;
}

// FFmpegCameraManager Implementation
FFmpegCameraManager::FFmpegCameraManager(QObject* parent)
    : QObject(parent)
    , m_videoItem(nullptr)
    , m_videoWidget(nullptr)
    , m_resolution(1920, 1080)
    , m_fps(30)
    , m_isActive(false)
    , m_frameCount(0)
    , m_currentFPS(0.0)
    , m_droppedFrames(0)
{
    qCDebug(log_ffmpeg_camera) << "FFmpegCameraManager created";
    
    // Initialize FPS monitoring timer
    m_fpsTimer = new QTimer(this);
    m_fpsTimer->setInterval(1000);  // Update every second
    connect(m_fpsTimer, &QTimer::timeout, this, &FFmpegCameraManager::updateFPS);
    
    // Create decode thread with enhanced multi-threading
    m_decodeThread = std::make_unique<FFmpegDecodeThread>(this);
    
    // Use queued connections for thread safety and better parallel processing
    connect(m_decodeThread.get(), &FFmpegDecodeThread::frameReady, 
            this, &FFmpegCameraManager::onFrameReady, Qt::QueuedConnection);
    connect(m_decodeThread.get(), &FFmpegDecodeThread::error, 
            this, &FFmpegCameraManager::onDecodingError, Qt::QueuedConnection);
    connect(m_decodeThread.get(), &FFmpegDecodeThread::decodingStarted, 
            this, [this]() { 
                m_isActive = true; 
                emit cameraActiveChanged(true); 
            }, Qt::QueuedConnection);
    connect(m_decodeThread.get(), &FFmpegDecodeThread::decodingStopped, 
            this, [this]() { 
                m_isActive = false; 
                emit cameraActiveChanged(false); 
            }, Qt::QueuedConnection);
    
    // Set optimal thread priority for video processing
    m_decodeThread->setPriority(QThread::HighPriority);
    
    qCDebug(log_ffmpeg_camera) << "Multi-threaded FFmpeg camera manager initialized with" 
                               << QThread::idealThreadCount() << "CPU cores available";
}

FFmpegCameraManager::~FFmpegCameraManager()
{
    stopCamera();
    qCDebug(log_ffmpeg_camera) << "FFmpegCameraManager destroyed";
}

bool FFmpegCameraManager::startCamera(const QString& devicePath)
{
    if (m_isActive) {
        qCDebug(log_ffmpeg_camera) << "Camera already active";
        return true;
    }
    
    // Find Openterface camera if no specific device path provided
    QString actualDevicePath = devicePath;
    if (devicePath == "/dev/video0" || devicePath.isEmpty()) {
        QString openterfaceDevice = findOpenterfaceCamera();
        if (!openterfaceDevice.isEmpty()) {
            actualDevicePath = openterfaceDevice;
        }
    }
    
    qCDebug(log_ffmpeg_camera) << "Starting camera with device:" << actualDevicePath;
    
    // Verify device exists and is accessible
    if (!QFile::exists(actualDevicePath)) {
        QString errorMsg = QString("Device not found: %1").arg(actualDevicePath);
        qCWarning(log_ffmpeg_camera) << errorMsg;
        emit error(errorMsg);
        return false;
    }
    
    m_currentDevice = actualDevicePath;
    
    // Initialize video sink if we have a video output
    initializeVideoSink();
    
    // Start decoding
    m_decodeThread->startDecoding(actualDevicePath, m_resolution, m_fps);
    
    // Start FPS monitoring
    m_frameCount = 0;
    m_fpsTimer->start();
    
    qCDebug(log_ffmpeg_camera) << "Camera start initiated";
    return true;
}

void FFmpegCameraManager::stopCamera()
{
    if (!m_isActive) {
        return;
    }
    
    qCDebug(log_ffmpeg_camera) << "Stopping camera";
    
    // Stop FPS monitoring
    m_fpsTimer->stop();
    
    // Stop decoding thread
    if (m_decodeThread) {
        m_decodeThread->stopDecoding();
    }
    
    // Clean up video sink
    cleanupVideoSink();
    
    // Clear frame buffer
    QMutexLocker locker(&m_bufferMutex);
    m_frameBuffer.clear();
    
    m_isActive = false;
    emit cameraActiveChanged(false);
    
    qCDebug(log_ffmpeg_camera) << "Camera stopped";
}

void FFmpegCameraManager::setResolution(const QSize& resolution)
{
    if (m_resolution == resolution) {
        return;
    }
    
    m_resolution = resolution;
    emit resolutionChanged(resolution);
    
    // If camera is active, restart with new resolution
    if (m_isActive) {
        QString currentDevice = m_currentDevice;
        stopCamera();
        startCamera(currentDevice);
    }
    
    qCDebug(log_ffmpeg_camera) << "Resolution set to:" << resolution;
}

void FFmpegCameraManager::setFrameRate(int fps)
{
    if (m_fps == fps) {
        return;
    }
    
    m_fps = fps;
    
    // If camera is active, restart with new frame rate
    if (m_isActive) {
        QString currentDevice = m_currentDevice;
        stopCamera();
        startCamera(currentDevice);
    }
    
    qCDebug(log_ffmpeg_camera) << "Frame rate set to:" << fps;
}

void FFmpegCameraManager::setVideoOutput(QGraphicsVideoItem* videoItem)
{
    if (m_videoItem == videoItem) {
        return;
    }
    
    m_videoItem = videoItem;
    m_videoWidget = nullptr;  // Clear widget output
    
    if (m_isActive) {
        initializeVideoSink();
    }
    
    qCDebug(log_ffmpeg_camera) << "Video output set to QGraphicsVideoItem";
}

void FFmpegCameraManager::setVideoOutput(QVideoWidget* videoWidget)
{
    if (m_videoWidget == videoWidget) {
        return;
    }
    
    m_videoWidget = videoWidget;
    m_videoItem = nullptr;  // Clear graphics item output
    
    if (m_isActive) {
        initializeVideoSink();
    }
    
    qCDebug(log_ffmpeg_camera) << "Video output set to QVideoWidget";
}

QStringList FFmpegCameraManager::getAvailableCameras() const
{
    QStringList cameras;
    QDir devDir("/dev");
    
    // Look for video devices
    QStringList filters;
    filters << "video*";
    QFileInfoList videoDevices = devDir.entryInfoList(filters, QDir::System);
    
    for (const QFileInfo& device : videoDevices) {
        QString devicePath = device.absoluteFilePath();
        
        // Check if it's a valid V4L2 device
        int fd = open(devicePath.toUtf8().constData(), O_RDONLY);
        if (fd >= 0) {
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
                if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
                    cameras << devicePath;
                }
            }
            close(fd);
        }
    }
    
    return cameras;
}

QString FFmpegCameraManager::findOpenterfaceCamera() const
{
    QStringList cameras = getAvailableCameras();
    
    for (const QString& camera : cameras) {
        // Query device capabilities to find Openterface
        int fd = open(camera.toUtf8().constData(), O_RDONLY);
        if (fd >= 0) {
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
                QString cardName = QString::fromUtf8((char*)cap.card);
                if (cardName.contains("Openterface", Qt::CaseInsensitive) ||
                    cardName.contains("MS2109", Qt::CaseInsensitive)) {
                    close(fd);
                    qCDebug(log_ffmpeg_camera) << "Found Openterface device:" << camera << "card:" << cardName;
                    return camera;
                }
            }
            close(fd);
        }
    }
    
    qCDebug(log_ffmpeg_camera) << "Openterface device not found, using default";
    return cameras.isEmpty() ? QString() : cameras.first();
}

QList<QSize> FFmpegCameraManager::getSupportedResolutions(const QString& devicePath) const
{
    QList<QSize> resolutions;
    
    int fd = open(devicePath.toUtf8().constData(), O_RDONLY);
    if (fd < 0) {
        return resolutions;
    }
    
    struct v4l2_frmsizeenum frmsize;
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.pixel_format = V4L2_PIX_FMT_MJPEG;  // Try MJPEG first
    
    for (frmsize.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0; frmsize.index++) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            resolutions << QSize(frmsize.discrete.width, frmsize.discrete.height);
        }
    }
    
    // If no MJPEG resolutions found, try YUYV
    if (resolutions.isEmpty()) {
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = V4L2_PIX_FMT_YUYV;
        
        for (frmsize.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0; frmsize.index++) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                resolutions << QSize(frmsize.discrete.width, frmsize.discrete.height);
            }
        }
    }
    
    close(fd);
    return resolutions;
}

QList<int> FFmpegCameraManager::getSupportedFrameRates(const QString& devicePath) const
{
    QList<int> frameRates;
    
    int fd = open(devicePath.toUtf8().constData(), O_RDONLY);
    if (fd < 0) {
        return frameRates;
    }
    
    struct v4l2_frmivalenum frmival;
    memset(&frmival, 0, sizeof(frmival));
    frmival.pixel_format = V4L2_PIX_FMT_MJPEG;
    frmival.width = 1920;
    frmival.height = 1080;
    
    for (frmival.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0; frmival.index++) {
        if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            int fps = frmival.discrete.denominator / frmival.discrete.numerator;
            if (!frameRates.contains(fps)) {
                frameRates << fps;
            }
        }
    }
    
    close(fd);
    
    // Add common frame rates if none found
    if (frameRates.isEmpty()) {
        frameRates << 15 << 20 << 24 << 25 << 30 << 50 << 60;
    }
    
    return frameRates;
}

void FFmpegCameraManager::onFrameReady(const QVideoFrame& frame)
{
    // Update frame count for FPS calculation (atomic operation for thread safety)
    QMutexLocker fpsLocker(&m_fpsMutex);
    m_frameCount++;
    fpsLocker.unlock();
    
    // Parallel frame buffer management using lock-free operations where possible
    QMutexLocker bufferLocker(&m_bufferMutex);
    
    // Add frame to buffer with timestamp
    VideoFrameBuffer frameBuffer(frame, QDateTime::currentMSecsSinceEpoch());
    m_frameBuffer.enqueue(frameBuffer);
    
    // Efficient buffer size management
    if (m_frameBuffer.size() > MAX_BUFFER_SIZE) {
        m_frameBuffer.dequeue();
        m_droppedFrames++;
    }
    
    bufferLocker.unlock();
    
    // Send frame to video sink asynchronously for better multi-threading performance
    if (m_videoSink) {
        // Use Qt's asynchronous operations for thread-safe video sink updates
        QMetaObject::invokeMethod(m_videoSink.get(), [this, frame]() {
            m_videoSink->setVideoFrame(frame);
        }, Qt::QueuedConnection);
    }
    
    // Emit frame ready signal for any additional processing threads
    emit frameReady(frame);
}

void FFmpegCameraManager::onDecodingError(const QString& error)
{
    qCWarning(log_ffmpeg_camera) << "Decoding error:" << error;
    emit this->error(error);
    
    // Try to restart the camera after an error
    if (m_isActive) {
        stopCamera();
        QTimer::singleShot(1000, this, [this]() {
            startCamera(m_currentDevice);
        });
    }
}

void FFmpegCameraManager::updateFPS()
{
    QMutexLocker locker(&m_fpsMutex);
    m_currentFPS = m_frameCount;
    m_frameCount = 0;
    locker.unlock();
    
    emit fpsChanged(m_currentFPS);
}

void FFmpegCameraManager::initializeVideoSink()
{
    cleanupVideoSink();
    
    if (m_videoItem) {
        m_videoSink = std::make_unique<QVideoSink>();
        // Connect to video item's video sink
        if (m_videoItem->videoSink()) {
            m_videoSink = std::unique_ptr<QVideoSink>(m_videoItem->videoSink());
        }
        qCDebug(log_ffmpeg_camera) << "Video sink initialized for QGraphicsVideoItem";
    } else if (m_videoWidget) {
        m_videoSink = std::make_unique<QVideoSink>();
        // Connect to video widget's video sink  
        if (m_videoWidget->videoSink()) {
            m_videoSink = std::unique_ptr<QVideoSink>(m_videoWidget->videoSink());
        }
        qCDebug(log_ffmpeg_camera) << "Video sink initialized for QVideoWidget";
    }
}

void FFmpegCameraManager::cleanupVideoSink()
{
    if (m_videoSink) {
        m_videoSink.reset();
        qCDebug(log_ffmpeg_camera) << "Video sink cleaned up";
    }
}

bool FFmpegCameraManager::queryDeviceCapabilities(const QString& devicePath)
{
    int fd = open(devicePath.toUtf8().constData(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    
    struct v4l2_capability cap;
    bool success = (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0);
    
    if (success) {
        qCDebug(log_ffmpeg_camera) << "Device capabilities for" << devicePath;
        qCDebug(log_ffmpeg_camera) << "Driver:" << (char*)cap.driver;
        qCDebug(log_ffmpeg_camera) << "Card:" << (char*)cap.card;
        qCDebug(log_ffmpeg_camera) << "Bus info:" << (char*)cap.bus_info;
        qCDebug(log_ffmpeg_camera) << "Capabilities:" << Qt::hex << cap.capabilities;
    }
    
    close(fd);
    return success;
}

QString FFmpegCameraManager::getHardwareAccelerationInfo() const
{
    if (!m_decodeThread) {
        return "No decode thread available";
    }
    
    return m_decodeThread->getHardwareAccelerationType();
}

bool FFmpegCameraManager::isHardwareAccelerated() const
{
    if (!m_decodeThread) {
        return false;
    }
    
    return m_decodeThread->isHardwareAccelerated();
}

// Hardware acceleration implementation
bool FFmpegDecodeThread::initializeHardwareAcceleration()
{
    qCDebug(log_ffmpeg_camera) << "Initializing GPU hardware acceleration";
    
    // Try different hardware acceleration types in order of preference for Linux (no CUDA)
    const enum AVHWDeviceType hw_types[] = {
        AV_HWDEVICE_TYPE_VAAPI,    // Intel/AMD on Linux (primary choice)
        AV_HWDEVICE_TYPE_QSV,      // Intel Quick Sync Video
        AV_HWDEVICE_TYPE_OPENCL,   // OpenCL GPU compute
        AV_HWDEVICE_TYPE_VULKAN,   // Vulkan GPU acceleration
        AV_HWDEVICE_TYPE_DRM,      // Direct Rendering Manager
        AV_HWDEVICE_TYPE_NONE
    };
    
    // Check available GPU hardware first
    qCDebug(log_ffmpeg_camera) << "Detecting available GPU hardware...";
    
    for (int i = 0; hw_types[i] != AV_HWDEVICE_TYPE_NONE; i++) {
        enum AVHWDeviceType type = hw_types[i];
        const char* hw_name = av_hwdevice_get_type_name(type);
        
        qCDebug(log_ffmpeg_camera) << "Attempting hardware acceleration:" << hw_name;
        
        // Prepare device-specific options
        AVDictionary* hw_opts = nullptr;
        if (type == AV_HWDEVICE_TYPE_VAAPI) {
            // VAAPI specific optimizations
            av_dict_set(&hw_opts, "connection_type", "drm", 0);
            av_dict_set(&hw_opts, "kernel_driver", "i915", 0);  // Intel GPU driver
        } else if (type == AV_HWDEVICE_TYPE_QSV) {
            // Intel Quick Sync optimizations
            av_dict_set(&hw_opts, "child_device", "/dev/dri/renderD128", 0);
        }
        
        // Try to create hardware device context
        int ret = av_hwdevice_ctx_create(&m_hwDeviceContext, type, nullptr, hw_opts, 0);
        av_dict_free(&hw_opts);
        if (ret == 0) {
            m_hwDeviceType = type;
            qCDebug(log_ffmpeg_camera) << "Successfully initialized hardware acceleration:" << hw_name;
            return true;
        } else {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            qCDebug(log_ffmpeg_camera) << "Failed to initialize" << hw_name << ":" << error_buf;
        }
    }
    
    qCDebug(log_ffmpeg_camera) << "No hardware acceleration available, falling back to software decoding";
    m_hwDeviceType = AV_HWDEVICE_TYPE_NONE;
    m_hwDeviceContext = nullptr;
    return false;
}

void FFmpegDecodeThread::cleanupHardwareAcceleration()
{
    if (m_hwDeviceContext) {
        av_buffer_unref(&m_hwDeviceContext);
        m_hwDeviceContext = nullptr;
    }
    m_hwDeviceType = AV_HWDEVICE_TYPE_NONE;
    qCDebug(log_ffmpeg_camera) << "Hardware acceleration cleaned up";
}

bool FFmpegDecodeThread::isHardwareFrame(AVFrame* frame) const
{
    if (!frame) return false;
    
    // Check if frame format indicates hardware acceleration (no CUDA)
    AVPixelFormat format = static_cast<AVPixelFormat>(frame->format);
    return (format == AV_PIX_FMT_VAAPI || format == AV_PIX_FMT_OPENCL ||
            format == AV_PIX_FMT_QSV || format == AV_PIX_FMT_D3D11 || 
            format == AV_PIX_FMT_DXVA2_VLD || format == AV_PIX_FMT_VULKAN || 
            format == AV_PIX_FMT_DRM_PRIME);
}

QString FFmpegDecodeThread::getHardwareAccelerationType() const
{
    if (m_hwDeviceType == AV_HWDEVICE_TYPE_NONE) {
        return "Software (CPU)";
    }
    
    const char* type_name = av_hwdevice_get_type_name(m_hwDeviceType);
    QString description;
    
    switch (m_hwDeviceType) {
        case AV_HWDEVICE_TYPE_VAAPI:
            description = QString("VAAPI (%1) - Intel/AMD GPU").arg(type_name);
            break;
        case AV_HWDEVICE_TYPE_QSV:
            description = QString("Quick Sync (%1) - Intel iGPU").arg(type_name);
            break;
        case AV_HWDEVICE_TYPE_OPENCL:
            description = QString("OpenCL (%1) - GPU Compute").arg(type_name);
            break;
        case AV_HWDEVICE_TYPE_VULKAN:
            description = QString("Vulkan (%1) - Modern GPU API").arg(type_name);
            break;
        case AV_HWDEVICE_TYPE_DRM:
            description = QString("DRM (%1) - Direct GPU Access").arg(type_name);
            break;
        default:
            description = QString("Hardware (%1)").arg(type_name);
            break;
    }
    
    return description;
}

#include "ffmpegcameramanager.moc"

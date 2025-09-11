/*
* ========================================================================== *
*                                                                            *
*    This file is part of the Openterface Mini KVM App QT version            *
*                                                                            *
*    Copyright (C) 2024   <info@openterface.com>                             *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation version 3.                                 *
*                                                                            *
*    This program is distributed in the hope that it will be useful, but     *
*    WITHOUT ANY WARRANTY; without even the implied warranty of              *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        *
*    General Public License for more details.                                *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see <http://www.gnu.org/licenses/>.    *
*                                                                            *
* ========================================================================== *
*/

#ifndef GSTREAMERBACKENDHANDLER_H
#define GSTREAMERBACKENDHANDLER_H

#include "../multimediabackend.h"
#include <QProcess>
#include <QWidget>
#include <QTimer>

// Forward declarations for Qt types
class QGraphicsVideoItem;
class QGraphicsView;
class VideoPane;

// Forward declarations for GStreamer types
typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstMessage GstMessage;

/**
 * @brief GStreamer backend handler implementation with direct pipeline support
 */
class GStreamerBackendHandler : public MultimediaBackendHandler
{
    Q_OBJECT

public:
    explicit GStreamerBackendHandler(QObject *parent = nullptr);
    ~GStreamerBackendHandler();

    MultimediaBackendType getBackendType() const override;
    QString getBackendName() const override;
    MultimediaBackendConfig getDefaultConfig() const override;

    void prepareCameraCreation(QCamera* oldCamera = nullptr) override;
    void configureCameraDevice(QCamera* camera, const QCameraDevice& device) override;
    void setupCaptureSession(QMediaCaptureSession* session, QCamera* camera) override;
    void prepareVideoOutputConnection(QMediaCaptureSession* session, QObject* videoOutput) override;
    void finalizeVideoOutputConnection(QMediaCaptureSession* session, QObject* videoOutput) override;
    void startCamera(QCamera* camera) override;
    void stopCamera(QCamera* camera) override;

    QList<int> getSupportedFrameRates(const QCameraFormat& format) const override;
    QCameraFormat selectOptimalFormat(const QList<QCameraFormat>& formats, 
                                    const QSize& resolution, 
                                    int desiredFrameRate,
                                    QVideoFrameFormat::PixelFormat pixelFormat) const override;

    void handleCameraError(QCamera::Error error, const QString& errorString) override;

    // Direct GStreamer pipeline methods (enhanced with working example approach)
    bool createGStreamerPipeline(const QString& device, const QSize& resolution, int framerate);
    bool startGStreamerPipeline();
    void stopGStreamerPipeline();
    void setVideoOutput(QWidget* widget);
    void setVideoOutput(QGraphicsVideoItem* videoItem);  // Support for QGraphicsVideoItem
    
    // Enhanced methods based on working example
    bool checkCameraAvailable(const QString& device = "/dev/video0");
    WId getVideoWidgetWindowId() const;
    
    // Resolution and framerate configuration
    void setResolutionAndFramerate(const QSize& resolution, int framerate);
    
    // Pipeline string generation
    QString generatePipelineString(const QString& device, const QSize& resolution, int framerate) const;

    // Video recording methods
    bool startRecording(const QString& outputPath, const QString& format = "mp4", int videoBitrate = 2000000) override;
    void stopRecording() override;
    void pauseRecording() override;
    void resumeRecording() override;
    bool isRecording() const override;
    QString getCurrentRecordingPath() const override;
    qint64 getRecordingDuration() const override;
    
    // Recording configuration
    struct RecordingConfig {
        QString outputPath;
        QString format = "mp4";          // mp4, avi, mov, mkv
        QString videoCodec = "x264enc";  // x264enc, x265enc, vp8enc, vp9enc
        int videoBitrate = 2000000;      // 2 Mbps default
        int videoQuality = 23;           // Quality setting
        bool useHardwareAcceleration = false;
    };
    
    void setRecordingConfig(const RecordingConfig& config);
    RecordingConfig getRecordingConfig() const;

private slots:
    void onPipelineMessage();
    void checkPipelineHealth();

private:
    // GStreamer pipeline components
    GstElement* m_pipeline;
    GstElement* m_source;
    GstElement* m_sink;
    GstBus* m_bus;
    
    // Recording pipeline components
    GstElement* m_recordingPipeline;
    GstElement* m_recordingTee;
    GstElement* m_recordingSink;
    
    // Qt integration
    QWidget* m_videoWidget;
    QGraphicsVideoItem* m_graphicsVideoItem;  // Support for QGraphicsVideoItem
    QTimer* m_healthCheckTimer;
    QProcess* m_gstProcess;  // Fallback for process-based approach
    
    // Pipeline state
    bool m_pipelineRunning;
    QString m_currentDevice;
    QSize m_currentResolution;
    int m_currentFramerate;
    
    // Recording state
    bool m_recordingActive;
    bool m_recordingPaused;
    QString m_recordingOutputPath;
    RecordingConfig m_recordingConfig;
    qint64 m_recordingStartTime;
    qint64 m_recordingPausedTime;
    qint64 m_totalPausedDuration;
    
    // Helper methods
    bool initializeGStreamer();
    void cleanupGStreamer();
    bool embedVideoInWidget(QWidget* widget);
    bool embedVideoInGraphicsView(QGraphicsView* view);
    void handleGStreamerMessage(GstMessage* message);
    
    // Recording helper methods
    bool createRecordingPipeline(const QString& outputPath, const QString& format, int videoBitrate);
    bool startRecordingPipeline();
    void stopRecordingPipeline();
    void cleanupRecordingPipeline();
    QString generateRecordingPipelineString(const QString& outputPath, const QString& format, int videoBitrate) const;
};

#endif // GSTREAMERBACKENDHANDLER_H

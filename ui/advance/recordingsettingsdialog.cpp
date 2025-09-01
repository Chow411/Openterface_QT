/*
* ========================================================================== *
*                                                                            *
*    This file i    // Video codec (only show codecs available in current FFmpeg build)
    layout->addWidget(new QLabel(tr("Video Codec:")), row, 0);
    m_videoCodecCombo = new QComboBox();
    m_videoCodecCombo->addItems({"mjpeg"}); // MJPEG encoder for AVI container creates playable video files
    layout->addWidget(m_videoCodecCombo, row++, 1);t of the Openterface Mini KVM App QT version            *
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

#include "recordingsettingsdialog.h"
#include "../../global.h"
#include "../../ui/globalsetting.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLineEdit>
#include <QProgressBar>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>
#include <QApplication>
#include <QStyle>

RecordingSettingsDialog::RecordingSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_ffmpegBackend(nullptr)
    , m_isRecording(false)
    , m_isPaused(false)
    , m_updateTimer(new QTimer(this))
{
    setWindowTitle(tr("Video Recording Settings"));
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(500, 600);
    
    setupUI();
    connectSignals();
    loadSettings();
    updateControlStates();
    
    // Update timer for recording info
    m_updateTimer->setInterval(100); // Update every 100ms
    connect(m_updateTimer, &QTimer::timeout, this, &RecordingSettingsDialog::updateRecordingInfo);
}

RecordingSettingsDialog::~RecordingSettingsDialog()
{
    if (m_isRecording) {
        onStopRecording();
    }
    saveSettings();
}

void RecordingSettingsDialog::setFFmpegBackend(FFmpegBackendHandler* backend)
{
    // Disconnect from previous backend
    if (m_ffmpegBackend) {
        disconnect(m_ffmpegBackend, nullptr, this, nullptr);
    }
    
    m_ffmpegBackend = backend;
    
    // Connect to new backend signals
    if (m_ffmpegBackend) {
        connect(m_ffmpegBackend, &FFmpegBackendHandler::recordingStarted,
                this, &RecordingSettingsDialog::onRecordingStarted);
        connect(m_ffmpegBackend, &FFmpegBackendHandler::recordingStopped,
                this, &RecordingSettingsDialog::onRecordingStopped);
        connect(m_ffmpegBackend, &FFmpegBackendHandler::recordingPaused,
                this, &RecordingSettingsDialog::onRecordingPaused);
        connect(m_ffmpegBackend, &FFmpegBackendHandler::recordingResumed,
                this, &RecordingSettingsDialog::onRecordingResumed);
        connect(m_ffmpegBackend, &FFmpegBackendHandler::recordingError,
                this, &RecordingSettingsDialog::onRecordingError);
        connect(m_ffmpegBackend, &FFmpegBackendHandler::recordingDurationChanged,
                this, &RecordingSettingsDialog::onRecordingDurationChanged);
        
        // Update current recording state
        m_isRecording = m_ffmpegBackend->isRecording();
        updateControlStates();
    }
}

void RecordingSettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Recording controls
    setupRecordingControls();
    mainLayout->addWidget(m_recordingGroup);
    
    // Video settings
    setupVideoSettings();
    mainLayout->addWidget(m_videoGroup);
    
    // Output settings
    setupOutputSettings();
    mainLayout->addWidget(m_outputGroup);
    
    // Control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_applyButton = new QPushButton(tr("Apply Settings"));
    m_resetButton = new QPushButton(tr("Reset to Defaults"));
    m_closeButton = new QPushButton(tr("Close"));
    
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
}

void RecordingSettingsDialog::setupRecordingControls()
{
    m_recordingGroup = new QGroupBox(tr("Recording Controls"));
    QGridLayout* layout = new QGridLayout(m_recordingGroup);
    
    // Control buttons
    m_startButton = new QPushButton(tr("Start Recording"));
    m_stopButton = new QPushButton(tr("Stop Recording"));
    m_pauseButton = new QPushButton(tr("Pause"));
    m_resumeButton = new QPushButton(tr("Resume"));
    
    // Status and progress
    m_statusLabel = new QLabel(tr("Status: Ready"));
    m_durationLabel = new QLabel(tr("Duration: 00:00:00"));
    m_recordingProgress = new QProgressBar();
    m_recordingProgress->setRange(0, 0); // Indeterminate progress
    m_recordingProgress->setVisible(false);
    
    // Layout
    layout->addWidget(m_startButton, 0, 0);
    layout->addWidget(m_stopButton, 0, 1);
    layout->addWidget(m_pauseButton, 0, 2);
    layout->addWidget(m_resumeButton, 0, 3);
    layout->addWidget(m_statusLabel, 1, 0, 1, 4);
    layout->addWidget(m_durationLabel, 2, 0, 1, 4);
    layout->addWidget(m_recordingProgress, 3, 0, 1, 4);
    
    // Connect signals
    connect(m_startButton, &QPushButton::clicked, this, &RecordingSettingsDialog::onStartRecording);
    connect(m_stopButton, &QPushButton::clicked, this, &RecordingSettingsDialog::onStopRecording);
    connect(m_pauseButton, &QPushButton::clicked, this, &RecordingSettingsDialog::onPauseRecording);
    connect(m_resumeButton, &QPushButton::clicked, this, &RecordingSettingsDialog::onResumeRecording);
}

void RecordingSettingsDialog::setupVideoSettings()
{
    m_videoGroup = new QGroupBox(tr("Video Settings"));
    QGridLayout* layout = new QGridLayout(m_videoGroup);
    
    int row = 0;
    
    // Video codec
    layout->addWidget(new QLabel(tr("Codec:")), row, 0);
    m_videoCodecCombo = new QComboBox();
    m_videoCodecCombo->addItems({"libx264", "libx265", "mpeg4"});
    layout->addWidget(m_videoCodecCombo, row++, 1);
    
    // Video quality preset
    layout->addWidget(new QLabel(tr("Quality:")), row, 0);
    m_videoQualityCombo = new QComboBox();
    m_videoQualityCombo->addItems({tr("Low"), tr("Medium"), tr("High"), tr("Ultra"), tr("Custom")});
    layout->addWidget(m_videoQualityCombo, row++, 1);
    
    // Video bitrate
    layout->addWidget(new QLabel(tr("Bitrate (kbps):")), row, 0);
    m_videoBitrateSpin = new QSpinBox();
    m_videoBitrateSpin->setRange(100, 50000);
    m_videoBitrateSpin->setValue(2000);
    m_videoBitrateSpin->setSuffix(" kbps");
    layout->addWidget(m_videoBitrateSpin, row++, 1);
    
    // Connect quality preset to update bitrate
    connect(m_videoQualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                switch(index) {
                    case 0: m_videoBitrateSpin->setValue(1000); break; // Low
                    case 1: m_videoBitrateSpin->setValue(2000); break; // Medium
                    case 2: m_videoBitrateSpin->setValue(5000); break; // High
                    case 3: m_videoBitrateSpin->setValue(10000); break; // Ultra
                    case 4: break; // Custom - don't change
                }
            });
}

/*
void RecordingSettingsDialog::setupAudioSettings()
{
    m_audioGroup = new QGroupBox(tr("Audio Settings"));
    QGridLayout* layout = new QGridLayout(m_audioGroup);
    
    int row = 0;
    
    // Audio codec
    layout->addWidget(new QLabel(tr("Codec:")), row, 0);
    m_audioCodecCombo = new QComboBox();
    m_audioCodecCombo->addItems({"aac", "mp3", "vorbis", "flac"});
    layout->addWidget(m_audioCodecCombo, row++, 1);
    
    // Audio bitrate
    layout->addWidget(new QLabel(tr("Bitrate (kbps):")), row, 0);
    m_audioBitrateSpin = new QSpinBox();
    m_audioBitrateSpin->setRange(32, 512);
    m_audioBitrateSpin->setValue(128);
    m_audioBitrateSpin->setSuffix(" kbps");
    layout->addWidget(m_audioBitrateSpin, row++, 1);
    
    // Sample rate
    layout->addWidget(new QLabel(tr("Sample Rate:")), row, 0);
    m_sampleRateCombo = new QComboBox();
    m_sampleRateCombo->addItems({"22050", "44100", "48000", "96000"});
    m_sampleRateCombo->setCurrentText("44100");
    layout->addWidget(m_sampleRateCombo, row++, 1);
}
*/

void RecordingSettingsDialog::setupOutputSettings()
{
    m_outputGroup = new QGroupBox(tr("Output Settings"));
    QGridLayout* layout = new QGridLayout(m_outputGroup);
    
    int row = 0;
    
    // Output format (create this first so generateDefaultOutputPath can use it)
    layout->addWidget(new QLabel(tr("Format:")), row, 0);
    m_formatCombo = new QComboBox();
    // Only add formats that are available in the custom FFmpeg build
    // Current build supports avi, mjpeg and rawvideo muxers
    // AVI with MJPEG creates playable video files
    m_formatCombo->addItems({"avi"});
    layout->addWidget(m_formatCombo, row++, 1);
    
    // Output path (now format combo is available)
    layout->addWidget(new QLabel(tr("Output Path:")), row, 0);
    QHBoxLayout* pathLayout = new QHBoxLayout();
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setText(generateDefaultOutputPath());
    m_browseButton = new QPushButton(tr("Browse..."));
    pathLayout->addWidget(m_outputPathEdit);
    pathLayout->addWidget(m_browseButton);
    layout->addLayout(pathLayout, row++, 1);
    
    connect(m_browseButton, &QPushButton::clicked, this, &RecordingSettingsDialog::onBrowseOutputPath);
    
    // Update output path extension when format changes
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QString currentPath = m_outputPathEdit->text();
        if (!currentPath.isEmpty()) {
            QFileInfo fileInfo(currentPath);
            QString baseName = fileInfo.completeBaseName();
            QString dir = fileInfo.dir().absolutePath();
            QString newFormat = m_formatCombo->currentText();
            QString newPath = QDir(dir).filePath(QString("%1.%2").arg(baseName, newFormat));
            m_outputPathEdit->setText(newPath);
        }
    });
}

void RecordingSettingsDialog::connectSignals()
{
    connect(m_applyButton, &QPushButton::clicked, this, &RecordingSettingsDialog::onApplySettings);
    connect(m_resetButton, &QPushButton::clicked, this, &RecordingSettingsDialog::onResetToDefaults);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::hide);
}

void RecordingSettingsDialog::onStartRecording()
{
    if (!m_ffmpegBackend) {
        QMessageBox::warning(this, tr("Error"), tr("No video backend available."));
        return;
    }
    
    if (m_isRecording) {
        QMessageBox::information(this, tr("Recording"), tr("Recording is already in progress."));
        return;
    }
    
    // Apply current settings first
    onApplySettings();
    
    QString outputPath = m_outputPathEdit->text().trimmed();
    if (outputPath.isEmpty()) {
        outputPath = generateDefaultOutputPath();
        m_outputPathEdit->setText(outputPath);
    }
    
    // Ensure the filename extension matches the selected format
    QString format = m_formatCombo->currentText();
    QFileInfo fileInfo(outputPath);
    QString baseName = fileInfo.completeBaseName();
    QString dir = fileInfo.dir().absolutePath();
    outputPath = QDir(dir).filePath(QString("%1.%2").arg(baseName, format));
    m_outputPathEdit->setText(outputPath); // Update the display
    
    // Ensure output directory exists
    QDir().mkpath(QFileInfo(outputPath).dir().absolutePath());
    
    int bitrate = m_videoBitrateSpin->value() * 1000; // Convert to bps
    
    bool success = m_ffmpegBackend->startRecording(outputPath, format, bitrate);
    if (!success) {
        QMessageBox::warning(this, tr("Recording Error"), 
                           tr("Failed to start recording. Please check the settings and try again."));
    }
}

void RecordingSettingsDialog::onStopRecording()
{
    if (!m_ffmpegBackend || !m_isRecording) {
        return;
    }
    
    m_ffmpegBackend->stopRecording();
}

void RecordingSettingsDialog::onPauseRecording()
{
    if (!m_ffmpegBackend || !m_isRecording || m_isPaused) {
        return;
    }
    
    m_ffmpegBackend->pauseRecording();
}

void RecordingSettingsDialog::onResumeRecording()
{
    if (!m_ffmpegBackend || !m_isRecording || !m_isPaused) {
        return;
    }
    
    m_ffmpegBackend->resumeRecording();
}

void RecordingSettingsDialog::onBrowseOutputPath()
{
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (defaultDir.isEmpty()) {
        defaultDir = QDir::homePath();
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Save Recording As"),
        QDir(defaultDir).filePath("openterface_recording.mp4"),
        tr("Video Files (*.mp4 *.avi *.mov *.mkv *.webm);;All Files (*)")
    );
    
    if (!fileName.isEmpty()) {
        m_outputPathEdit->setText(fileName);
    }
}

void RecordingSettingsDialog::onApplySettings()
{
    if (!m_ffmpegBackend) {
        QMessageBox::warning(this, tr("Warning"), tr("No FFmpeg backend available!"));
        return;
    }
    
    FFmpegBackendHandler::RecordingConfig config;
    config.outputPath = m_outputPathEdit->text();
    config.format = m_formatCombo->currentText();
    config.videoCodec = m_videoCodecCombo->currentText();
    config.videoBitrate = m_videoBitrateSpin->value() * 1000; // Convert to bps
    config.videoQuality = 23; // Use default CRF value
    config.useHardwareAcceleration = false; // Default to false for compatibility
    
    m_ffmpegBackend->setRecordingConfig(config);
    saveSettings();
    
    m_statusLabel->setText(tr("Status: Settings applied"));
}

void RecordingSettingsDialog::onResetToDefaults()
{
    m_videoCodecCombo->setCurrentText("mjpeg");
    m_videoQualityCombo->setCurrentIndex(1); // Medium
    m_videoBitrateSpin->setValue(2000);
    
    m_formatCombo->setCurrentText("avi");
    m_outputPathEdit->setText(generateDefaultOutputPath());
}

void RecordingSettingsDialog::onRecordingStarted(const QString& outputPath)
{
    m_isRecording = true;
    m_isPaused = false;
    m_currentOutputPath = outputPath;
    m_recordingTimer.start();
    m_updateTimer->start();
    m_recordingProgress->setVisible(true);
    
    m_statusLabel->setText(tr("Status: Recording to %1").arg(QFileInfo(outputPath).fileName()));
    updateControlStates();
}

void RecordingSettingsDialog::onRecordingStopped()
{
    m_isRecording = false;
    m_isPaused = false;
    m_updateTimer->stop();
    m_recordingProgress->setVisible(false);
    
    m_statusLabel->setText(tr("Status: Recording stopped. File saved to %1").arg(QFileInfo(m_currentOutputPath).fileName()));
    m_durationLabel->setText(tr("Duration: %1").arg(formatDuration(m_recordingTimer.elapsed())));
    updateControlStates();
}

void RecordingSettingsDialog::onRecordingPaused()
{
    m_isPaused = true;
    m_statusLabel->setText(tr("Status: Recording paused"));
    updateControlStates();
}

void RecordingSettingsDialog::onRecordingResumed()
{
    m_isPaused = false;
    m_statusLabel->setText(tr("Status: Recording resumed"));
    updateControlStates();
}

void RecordingSettingsDialog::onRecordingError(const QString& error)
{
    m_isRecording = false;
    m_isPaused = false;
    m_updateTimer->stop();
    m_recordingProgress->setVisible(false);
    
    m_statusLabel->setText(tr("Status: Recording error - %1").arg(error));
    updateControlStates();
    
    QMessageBox::critical(this, tr("Recording Error"), 
                         tr("Recording failed: %1").arg(error));
}

void RecordingSettingsDialog::onRecordingDurationChanged(qint64 duration)
{
    m_durationLabel->setText(tr("Duration: %1").arg(formatDuration(duration)));
}

void RecordingSettingsDialog::updateRecordingInfo()
{
    if (m_isRecording && m_ffmpegBackend) {
        qint64 duration = m_ffmpegBackend->getRecordingDuration();
        if (duration > 0) {
            m_durationLabel->setText(tr("Duration: %1").arg(formatDuration(duration)));
        }
    }
}

void RecordingSettingsDialog::updateControlStates()
{
    m_startButton->setEnabled(!m_isRecording);
    m_stopButton->setEnabled(m_isRecording);
    m_pauseButton->setEnabled(m_isRecording && !m_isPaused);
    m_resumeButton->setEnabled(m_isRecording && m_isPaused);
    
    // Disable settings while recording
    bool settingsEnabled = !m_isRecording;
    m_videoGroup->setEnabled(settingsEnabled);
    m_outputGroup->setEnabled(settingsEnabled);
    m_applyButton->setEnabled(settingsEnabled);
    m_resetButton->setEnabled(settingsEnabled);
}

void RecordingSettingsDialog::loadSettings()
{
    // Load settings from GlobalSetting or use defaults
    GlobalSetting& settings = GlobalSetting::instance();
    
    m_videoCodecCombo->setCurrentText(settings.getRecordingVideoCodec());
    m_videoBitrateSpin->setValue(settings.getRecordingVideoBitrate() / 1000);
    
    m_formatCombo->setCurrentText(settings.getRecordingOutputFormat());
    
    QString savedPath = settings.getRecordingOutputPath();
    if (savedPath.isEmpty()) {
        savedPath = generateDefaultOutputPath();
    }
    m_outputPathEdit->setText(savedPath);
}

void RecordingSettingsDialog::saveSettings()
{
    GlobalSetting& settings = GlobalSetting::instance();
    
    settings.setRecordingVideoCodec(m_videoCodecCombo->currentText());
    settings.setRecordingVideoBitrate(m_videoBitrateSpin->value() * 1000);
    settings.setRecordingOutputFormat(m_formatCombo->currentText());
    settings.setRecordingOutputPath(m_outputPathEdit->text());
}

QString RecordingSettingsDialog::formatDuration(qint64 milliseconds)
{
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString RecordingSettingsDialog::generateDefaultOutputPath()
{
    QString videosDir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (videosDir.isEmpty()) {
        videosDir = QDir::homePath();
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    
    // Default to avi if format combo is not yet created or has no selection
    // (Only formats available in the current FFmpeg build)
    QString format = "avi";
    if (m_formatCombo && m_formatCombo->count() > 0) {
        format = m_formatCombo->currentText();
    }
    
    // Set appropriate file extension based on format
    QString extension;
    if (format == "avi") {
        extension = "avi";  // AVI container with MJPEG creates playable video files
    } else if (format == "rawvideo") {
        extension = "yuv";  // Raw video typically uses .yuv extension
    } else if (format == "mjpeg") {
        extension = "mjpeg"; // MJPEG single image format
    } else {
        extension = format;  // For other formats, use format name as extension
    }
    
    return QDir(videosDir).filePath(QString("openterface_recording_%1.%2").arg(timestamp, extension));
}

void RecordingSettingsDialog::showDialog()
{
    if (!isVisible()) {
        show();
        raise();
        activateWindow();
    } else {
        raise();
        activateWindow();
    }
}

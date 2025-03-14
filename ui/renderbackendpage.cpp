#include "renderbackendpage.h"
#include <QVBoxLayout>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>
extern "C"{
    #include <libavutil/hwcontext.h>  // FFmpeg header
}

RenderBackendPage::RenderBackendPage(QWidget *parent) : QWidget(parent) {
    setupUi();
    checkSupportedBackends(); // Check supported FFmpeg hardware acceleration backends and update UI
    initRenderSettings();    // Initialize settings
}

void RenderBackendPage::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("FFmpeg Hardware Acceleration Backend (Restart Required):", this));

    renderBackendCombo = new QComboBox(this);
    layout->addWidget(renderBackendCombo);

    supportLabel = new QLabel("Select a hardware acceleration backend supported by your system.", this);
    layout->addWidget(supportLabel);

    layout->addStretch();

    // Connect signal to slot for real-time response to user selection
    connect(renderBackendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RenderBackendPage::onBackendChanged);
}

void RenderBackendPage::checkSupportedBackends() {
    qDebug() << "Checking supported FFmpeg hardware acceleration backends...";
    
    renderBackendCombo->addItem("Software (No Hardware Acceleration)", QString(""));
    
    AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NONE;
    while ((hwType = av_hwdevice_iterate_types(hwType)) != AV_HWDEVICE_TYPE_NONE) {
        const char *typeName = av_hwdevice_get_type_name(hwType);
        if (typeName) {
            QString backendName = QString(typeName).toUpper();
            if (isQtSupportedBackend(typeName)) {
                renderBackendCombo->addItem(backendName, QString(typeName));
                qDebug() << "Found supported backend:" << typeName;
            }
        }
    }
    
    if (renderBackendCombo->count() == 1) {
        supportLabel->setText("No hardware acceleration backends detected or supported by Qt on your system.");
    }
}

bool RenderBackendPage::isQtSupportedBackend(const QString &backend) {
    static const QSet<QString> supportedBackends = {
        "vaapi", "dxva2", "d3d11va", "cuda", "vdpau", "qsv", "opencl", "vulkan", "drm"
    };
    return supportedBackends.contains(backend.toLower());
}

void RenderBackendPage::initRenderSettings() {
    QSettings settings("Techxartisan", "Openterface");

    // Load the last FFmpeg hardware acceleration backend from settings, default to empty (software decoding)
    QString lastBackend = settings.value("render/ffmpeg_hw_backend", "").toString();

    // Find and set the last backend in the ComboBox
    int index = renderBackendCombo->findData(lastBackend);
    if (index != -1) {
        renderBackendCombo->setCurrentIndex(index);
    } else {
        renderBackendCombo->setCurrentIndex(0); // Default to "Software"
    }

    qDebug() << "Initialized FFmpeg hardware backend from settings:" << lastBackend;
}

void RenderBackendPage::applyRenderSettings() {
    QSettings settings("Techxartisan", "Openterface");

    // Get the current hardware acceleration backend
    QString currentBackend = renderBackendCombo->currentData().toString();
    QString lastBackend = settings.value("render/ffmpeg_hw_backend", "").toString();

    // Save and apply changes if the backend has changed
    if (lastBackend != currentBackend) {
        settings.setValue("render/ffmpeg_hw_backend", currentBackend);

        // Set environment variable to apply hardware acceleration
        if (currentBackend.isEmpty()) {
            qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", ""); // Disable hardware acceleration
            qDebug() << "Disabled FFmpeg hardware acceleration.";
        } else {
            qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", currentBackend.toUtf8());
            QString backend =  qgetenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES").toUtf8();
            qDebug() << "Set FFmpeg hardware acceleration to:" << backend;
        }

        QMessageBox::information(this, "Restart Required", 
                                 "Please restart the application for the FFmpeg hardware acceleration changes to take effect.");
    } else {
        qDebug() << "No changes to FFmpeg hardware backend.";
    }
}

void RenderBackendPage::onBackendChanged(int index) {
    QString backend = renderBackendCombo->itemData(index).toString();
    qDebug() << "Selected FFmpeg hardware backend:" << renderBackendCombo->currentText() 
             << "mapped to:" << (backend.isEmpty() ? "Software" : backend);
}
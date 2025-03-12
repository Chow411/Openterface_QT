#include "renderbackendpage.h"
#include <QVBoxLayout>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>
#include <QOpenGLContext>
#include <QOperatingSystemVersion>

RenderBackendPage::RenderBackendPage(QWidget *parent) : QWidget(parent) {
    setupUi();
    checkSupportedBackends(); // Check supported backends and update UI
}

void RenderBackendPage::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Video Rendering Backend (Restart Required):", this));

    renderBackendCombo = new QComboBox(this);
    layout->addWidget(renderBackendCombo);

    supportLabel = new QLabel("Select a backend supported by your system.", this);
    layout->addWidget(supportLabel);

    // Load previous settings
    QSettings settings("MyApp", "RendererSettings");
    QString lastBackend = settings.value("RenderingBackend", "opengl").toString();
    int index = renderBackendCombo->findData(lastBackend);
    if (index != -1) {
        renderBackendCombo->setCurrentIndex(index);
    }

    layout->addStretch();

    // Connect signal to slot for real-time response to user selection
    connect(renderBackendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RenderBackendPage::onBackendChanged);
}

void RenderBackendPage::checkSupportedBackends() {
    // Check operating system
    QOperatingSystemVersion os = QOperatingSystemVersion::current();

    // OpenGL support detection
    QOpenGLContext context;
    if (context.create()) {
        renderBackendCombo->addItem("OpenGL", QVariant::fromValue(QSGRendererInterface::OpenGL));
    }

    // Direct3D 11 and 12 (Windows only)
    if (os.type() == QOperatingSystemVersion::Windows) {
        if (os >= QOperatingSystemVersion::Windows7) {
            renderBackendCombo->addItem("DirectX 11", QVariant::fromValue(QSGRendererInterface::Direct3D11));
        }
    }

    // Vulkan support (basic check)
    renderBackendCombo->addItem("Vulkan", QVariant::fromValue(QSGRendererInterface::Vulkan));
    renderBackendCombo->addItem("Null", QVariant::fromValue(QSGRendererInterface::Null));
    // If no backends are supported (unlikely), notify the user
    if (renderBackendCombo->count() == 0) {
        supportLabel->setText("No supported rendering backends detected.");
        renderBackendCombo->setEnabled(false);
    } else {
        // Ensure the current selection is valid based on the stored string identifier
        QSettings settings("MyApp", "RendererSettings");
        QString currentBackend = settings.value("RenderingBackend", "opengl").toString();
        int index = renderBackendCombo->findData(mapStringToGraphicsApi(currentBackend));
        if (index == -1) {
            renderBackendCombo->setCurrentIndex(0); // Default to first supported backend
            qDebug() << "Switched to supported backend:" << renderBackendCombo->currentText();
        } else {
            renderBackendCombo->setCurrentIndex(index);
        }
    }
}

void RenderBackendPage::initRenderSettings() {
    QSettings settings("Techxartisan", "Openterface");
    QSGRendererInterface::GraphicsApi lastbackend = settings.value("render/backend", QSGRendererInterface::OpenGL).value<QSGRendererInterface::GraphicsApi>();
    int index = renderBackendCombo->findData(lastbackend);
    renderBackendCombo->setCurrentIndex(index);
}

void RenderBackendPage::applyRenderSettings() {
    // Left empty for you to fill in
    QSettings settings("Techxartisan", "Openterface");
    QSGRendererInterface::GraphicsApi lastbackend = settings.value("render/backend", QSGRendererInterface::OpenGL).value<QSGRendererInterface::GraphicsApi>();
    QSGRendererInterface::GraphicsApi backend = renderBackendCombo->currentData().value<QSGRendererInterface::GraphicsApi>();
    if (lastbackend != backend) {
        settings.setValue("render/backend", backend);
        QMessageBox::information(this, "Restart Required", "Please restart the application for the changes to take effect.");
    }
    qDebug() << "Applying rendering settings...";

}

void RenderBackendPage::onBackendChanged(int index) {
    QSGRendererInterface::GraphicsApi api = renderBackendCombo->itemData(index).value<QSGRendererInterface::GraphicsApi>();
    qDebug() << "Selected backend:" << renderBackendCombo->currentText() << "mapped to QSGRendererInterface::GraphicsApi:" << api;
}

#ifndef UI_RENDERBACKENDPAGE_H_
#define UI_RENDERBACKENDPAGE_H_

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QSGRendererInterface>

class RenderBackendPage : public QWidget {
    Q_OBJECT
public:
    explicit RenderBackendPage(QWidget *parent = nullptr);
    void applyRenderSettings();
    void initRenderSettings();

private:
    void setupUi();
    void checkSupportedBackends();
    inline QSGRendererInterface::GraphicsApi mapStringToGraphicsApi(const QString &backend){
        if (backend == "OpenGL") return QSGRendererInterface::OpenGL;
        if (backend == "Direct3D11") return QSGRendererInterface::Direct3D11;
        if (backend == "Vulkan") return QSGRendererInterface::Vulkan;
        if (backend == "Null") return QSGRendererInterface::Null;
        return QSGRendererInterface::Null; // Default fallback
    };

    QComboBox *renderBackendCombo;
    QLabel *supportLabel;    

private slots:
    void onBackendChanged(int index);
};

#endif // UI_RENDERBACKENDPAGE_H_

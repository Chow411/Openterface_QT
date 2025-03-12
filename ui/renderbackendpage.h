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

private:
    void setupUi();
    void checkSupportedBackends();
    QSGRendererInterface::GraphicsApi mapStringToGraphicsApi(const QString &backend);

    QComboBox *renderBackendCombo;
    QLabel *supportLabel;

public slots:
    void applySettings();

private slots:
    void onBackendChanged(int index);
};

#endif // UI_RENDERBACKENDPAGE_H_

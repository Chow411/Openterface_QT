#ifndef UI_RENDERBACKENDPAGE_H_
#define UI_RENDERBACKENDPAGE_H_

#include <QWidget>
#include <QComboBox>
#include <QLabel>


class RenderBackendPage : public QWidget {
    Q_OBJECT

public:
    explicit RenderBackendPage(QWidget *parent = nullptr);
    void initRenderSettings();
    void applyRenderSettings();

private:
    void setupUi();
    void checkSupportedBackends();
    bool isQtSupportedBackend(const QString &backend);

private slots:
    void onBackendChanged(int index);
    
private:
    QComboBox *renderBackendCombo;
    QLabel *supportLabel;
};

#endif // UI_RENDERBACKENDPAGE_H_

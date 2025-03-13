#ifndef UI_RENDERBACKENDPAGE_H_
#define UI_RENDERBACKENDPAGE_H_

#include <QWidget>
#include <QComboBox>
#include <QLabel>


class RenderBackendPage : public QWidget {
    Q_OBJECT

public:
    explicit RenderBackendPage(QWidget *parent = nullptr);

private:
    void setupUi();
    void checkSupportedBackends();
    void initRenderSettings();
    void applyRenderSettings();

private slots:
    void onBackendChanged(int index);

private:
    QComboBox *renderBackendCombo;
    QLabel *supportLabel;
};

#endif // UI_RENDERBACKENDPAGE_H_

#ifndef SCREENSCALE_H
#define SCREENSCALE_H

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>

class ScreenScale : public QDialog
{
    Q_OBJECT

public:
    explicit ScreenScale(QWidget *parent = nullptr);
    ~ScreenScale();

    QString getSelectedRatio() const;

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    QComboBox *ratioComboBox;
    QPushButton *okButton;
    QPushButton *cancelButton;
    QVBoxLayout *layout;
    QHBoxLayout *layoutBtn;
};

#endif // SCREENSCALE_H
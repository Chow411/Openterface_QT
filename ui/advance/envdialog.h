#ifndef ENVIRONMENTSETUPDIALOG_H
#define ENVIRONMENTSETUPDIALOG_H

#include <QDialog>
#include <QCloseEvent>
#include <libusb-1.0/libusb.h>

namespace Ui {
class EnvironmentSetupDialog;
}

class EnvironmentSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EnvironmentSetupDialog(QWidget *parent = nullptr);
    ~EnvironmentSetupDialog();
    
    // Static method to check if the CH340 driver is installed
    static bool checkEnvironmentSetup();

    static bool autoEnvironmentCheck();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void accept() override;
    void reject() override;
    void extractDriverFiles();
    void copyCommands();
    void openHelpLink();
#ifdef _WIN32
    void installDriverForWindows();
#endif

private:
    Ui::EnvironmentSetupDialog *ui;
    static bool checkDriverInstalled();
    static const QString helpUrl;

    static bool isDriverInstalled;
    static const QString tickHtml;
    static const QString crossHtml;
    bool isDevicePlugged;
    const uint16_t openterfaceVID = 0x534D;
    const uint16_t openterfacePID = 0x2109;
    const uint16_t ch340VID = 0x1A86;
    const uint16_t ch340PID = 0x7523;
    libusb_context *context = nullptr;
    bool detectDevice(uint16_t vendorID, uint16_t productID);

#ifdef __linux__
    static bool checkInRightUserGroup(); 
    static bool checkHidPermission();
    static bool checkBrlttyRunning();

    static const QString driverCommands;
    static const QString groupCommands;
    static const QString udevCommands;
    static const QString brlttyCommands;

    static bool isInRightUserGroup;
    static bool isHidPermission;
    static bool isBrlttyRunning;

    QString buildCommands();
#endif
};

#endif // ENVIRONMENTSETUPDIALOG_H
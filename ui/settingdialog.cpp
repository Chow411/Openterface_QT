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

#include "settingdialog.h"
#include "ui_settingdialog.h"
#include "ui/logpage.h"
#include "ui/hardwarepage.h"
#include "host/cameramanager.h"
#include "ui/videopage.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QRegularExpression>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStackedWidget>
#include <QDebug>
#include <QLoggingCategory>
#include <QSettings>
#include <QElapsedTimer>
#include <qtimer.h>
#include <QList>
#include <QSerialPortInfo>
#include <QLineEdit>
#include <QByteArray>


SettingDialog::SettingDialog(CameraManager *cameraManager, QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingDialog), m_cameraManager(cameraManager),
      settingTree(new QTreeWidget(this)), stackedWidget(new QStackedWidget(this)),
      logPage(new LogPage(this)), audioPage(new AudioPage(this)),
      videoPage(new VideoPage(cameraManager, this)), hardwarePage(new HardwarePage(this)),
      #ifdef Q_OS_LINUX
        renderBackendPage(new RenderBackendPage(this)),
      #endif
      buttonWidget(new QWidget(this)) {
    ui->setupUi(this);
    createSettingTree();
    createPages();
    createButtons();
    createLayout();

    setWindowTitle(tr("Preferences"));
    logPage->initLogSettings();
    videoPage->initVideoSettings();
    hardwarePage->initHardwareSetting();
    #ifdef Q_OS_LINUX
        renderBackendPage->initRenderSettings();
    #endif
    connect(settingTree, &QTreeWidget::currentItemChanged, this, &SettingDialog::changePage);
}

SettingDialog::~SettingDialog() {
    delete ui;
}

void SettingDialog::createSettingTree() {
    settingTree->setColumnCount(1);
    settingTree->setHeaderHidden(true);
    settingTree->setSelectionMode(QAbstractItemView::SingleSelection);
    settingTree->setMaximumSize(QSize(120, 1000));
    settingTree->setRootIsDecorated(false);

    QStringList names = {"General", "Video", "Audio", "Hardware", "Rendering"};
    for (const QString &name : names) {
        QTreeWidgetItem *item = new QTreeWidgetItem(settingTree);
        item->setText(0, name);
    }
}

void SettingDialog::createPages() {
    stackedWidget->addWidget(logPage);
    stackedWidget->addWidget(videoPage);
    stackedWidget->addWidget(audioPage);
    stackedWidget->addWidget(hardwarePage);
    #ifdef Q_OS_LINUX
    stackedWidget->addWidget(renderBackendPage);
    #endif
}

void SettingDialog::createButtons() {
    QPushButton *okButton = new QPushButton("OK");
    QPushButton *applyButton = new QPushButton("Apply");
    QPushButton *cancelButton = new QPushButton("Cancel");

    okButton->setFixedSize(80, 30);
    applyButton->setFixedSize(80, 30);
    cancelButton->setFixedSize(80, 30);

    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(cancelButton);

    connect(okButton, &QPushButton::clicked, this, &SettingDialog::handleOkButton);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(applyButton, &QPushButton::clicked, this, &SettingDialog::applyAccrodingPage);
}

void SettingDialog::createLayout() {
    QHBoxLayout *selectLayout = new QHBoxLayout;
    selectLayout->addWidget(settingTree);
    selectLayout->addWidget(stackedWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(selectLayout);
    mainLayout->addWidget(buttonWidget);

    setLayout(mainLayout);
}

void SettingDialog::changePage(QTreeWidgetItem *current, QTreeWidgetItem *previous) {
    static bool isChanging = false;
    if (isChanging) return;

    isChanging = true;
    if (!current) current = previous;

    QString itemText = current->text(0);
    qDebug() << "Selected item:" << itemText;

    if (itemText == "General") {
        stackedWidget->setCurrentIndex(0);
    } else if (itemText == "Video") {
        stackedWidget->setCurrentIndex(1);
    } else if (itemText == "Audio") {
        stackedWidget->setCurrentIndex(2);
    } else if (itemText == "Hardware") {
        stackedWidget->setCurrentIndex(3);
    } else if (itemText == "Rendering") {
        stackedWidget->setCurrentIndex(4);
    }

    QTimer::singleShot(100, this, [this]() { isChanging = false; });
}

void SettingDialog::applyAccrodingPage() {
    int currentPageIndex = stackedWidget->currentIndex();
    switch (currentPageIndex) {
    case 0: // General
        logPage->applyLogsettings();
        break;
    case 1: // Video
        videoPage->applyVideoSettings();
        break;
    case 2: // Audio
        break;
    case 3: // Hardware
        hardwarePage->applyHardwareSetting();
        break;
    #ifdef Q_OS_LINUX
        case 4: // Rendering
            renderBackendPage->applyRenderSettings();
            break;
    #endif
    default:
        break;
    }
}

void SettingDialog::handleOkButton() {
    logPage->applyLogsettings();
    videoPage->applyVideoSettings();
    hardwarePage->applyHardwareSetting();
    #ifdef Q_OS_LINUX
        renderBackendPage->applyRenderSettings();
    #endif
    accept();
}

HardwarePage* SettingDialog::getHardwarePage() { return hardwarePage; }
VideoPage* SettingDialog::getVideoPage() { return videoPage; }

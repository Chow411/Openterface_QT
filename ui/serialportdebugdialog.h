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


#ifndef SERIALPORTDEBUGDIALOG_H
#define SERIALPORTDEBUGDIALOG_H
#include <QDialog>
#include <QTextEdit>
#include <QWidget>
#include <QByteArray>
#include <QString>
#include <QCheckBox>
#include <QSettings>
#include <QDateTime>
namespace Ui {
class SerialPortDebugDialog;
}
class SerialPortDebugDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SerialPortDebugDialog(QWidget *parent = nullptr);
    ~SerialPortDebugDialog();
private slots:
    void getRecvDataAndInsertText(const QByteArray &data);
    void getSentDataAndInsertText(const QByteArray &data);


private:
    Ui::SerialPortDebugDialog *ui;
    QTextEdit *textEdit;
    QWidget *debugButtonWidget;
    QWidget *filterCheckboxWidget;
    void createDebugButtonWidget();
    void createFilterCheckBox();
    void saveSettings();
    void loadSettings();
    void createLayout();

    QString formatHexData(QString hexString);
};
#endif // SERIALPORTDEBUGDIALOG_H

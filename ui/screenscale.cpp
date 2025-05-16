#include "screenscale.h"

ScreenScale::ScreenScale(QWidget *parent) : QDialog(parent)
{
    // Set dialog title
    setWindowTitle(tr("Screen Aspect Ratio"));

    // Initialize widgets
    ratioComboBox = new QComboBox(this);
    okButton = new QPushButton("OK", this);
    cancelButton = new QPushButton("Cancel", this);

    // Populate combo box with common screen aspect ratios
    ratioComboBox->addItem("16:9");
    ratioComboBox->addItem("4:3");
    ratioComboBox->addItem("16:10");
    ratioComboBox->addItem("5:3");
    ratioComboBox->addItem("5:4");
    ratioComboBox->addItem("21:9");
    ratioComboBox->addItem("9:16");
    ratioComboBox->addItem("9:19.5");
    ratioComboBox->addItem("9:20");
    ratioComboBox->addItem("9:21");

    setFixedSize(200, 150);
    // Set up layout
    layout = new QVBoxLayout(this);
    layout->addWidget(ratioComboBox);
    layoutBtn = new QHBoxLayout(this);
    layoutBtn->addWidget(okButton);
    layoutBtn->addWidget(cancelButton);
    layout->addLayout(layoutBtn);

    // Connect buttons to slots
    connect(okButton, &QPushButton::clicked, this, &ScreenScale::onOkClicked);
    connect(cancelButton, &QPushButton::clicked, this, &ScreenScale::onCancelClicked);

    // Set layout to dialog
    setLayout(layout);
}

ScreenScale::~ScreenScale()
{
    // Qt handles widget deletion via parent-child hierarchy
}

QString ScreenScale::getSelectedRatio() const
{
    return ratioComboBox->currentText();
}

void ScreenScale::onOkClicked()
{
    accept(); // Close dialog with QDialog::Accepted status
}

void ScreenScale::onCancelClicked()
{
    reject(); // Close dialog with QDialog::Rejected status
}
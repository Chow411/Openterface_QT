#include "cornerwidgetmanager.h"
#include <QMenuBar>
#include <QDebug>
#include <QApplication>

CornerWidgetManager::CornerWidgetManager(QWidget *parent)
    : QObject(parent),
      cornerWidget(new QWidget(parent)),
      keyboardLayoutComboBox(nullptr),
      screenScaleButton(nullptr),
      zoomInButton(nullptr),
      zoomOutButton(nullptr),
      zoomReductionButton(nullptr),
      virtualKeyboardButton(nullptr),
      captureButton(nullptr),
      fullScreenButton(nullptr),
      pasteButton(nullptr),
      screensaverButton(nullptr),
      toggleSwitch(new ToggleSwitch(cornerWidget)),
      horizontalLayout(new QHBoxLayout()),
      menuBar(nullptr),
      layoutThreshold(800)
{
    createWidgets();
    setupConnections();
    horizontalLayout->setSpacing(2);
    horizontalLayout->setContentsMargins(5, 5, 5, 5);
    cornerWidget->setLayout(horizontalLayout);
    cornerWidget->adjustSize();
    cornerWidget->show();
}

CornerWidgetManager::~CornerWidgetManager()
{
    delete cornerWidget;
}

QWidget* CornerWidgetManager::getCornerWidget() const
{
    return cornerWidget;
}

void CornerWidgetManager::setMenuBar(QMenuBar *menuBar)
{
    this->menuBar = menuBar;
    if (menuBar) {
        menuBar->setCornerWidget(cornerWidget, Qt::TopRightCorner);
    }
}

void CornerWidgetManager::createWidgets()
{
    keyboardLayoutComboBox = new QComboBox(cornerWidget);
    keyboardLayoutComboBox->setObjectName("keyboardLayoutComboBox");
    keyboardLayoutComboBox->setFixedHeight(30);
    keyboardLayoutComboBox->setMinimumWidth(100);
    keyboardLayoutComboBox->setToolTip(tr("Select Keyboard Layout"));

    const struct {
        QPushButton** button;
        const char* objectName;
        const char* iconPath;
        const char* tooltip;
    } buttons[] = {
        {&screenScaleButton, "ScreenScaleButton", ":/images/screen_scale.svg", "Screen scale"},
        {&zoomInButton, "ZoomInButton", ":/images/zoom_in.svg", "Zoom in"},
        {&zoomOutButton, "ZoomOutButton", ":/images/zoom_out.svg", "Zoom out"},
        {&zoomReductionButton, "ZoomReductionButton", ":/images/zoom_fit.svg", "Restore original size"},
        {&virtualKeyboardButton, "virtualKeyboardButton", ":/images/keyboard.svg", "Function key and composite key"},
        {&captureButton, "captureButton", ":/images/capture.svg", "Full screen capture"},
        {&fullScreenButton, "fullScreenButton", ":/images/full_screen.svg", "Full screen mode"},
        {&pasteButton, "pasteButton", ":/images/paste.svg", "Paste text to target"},
        {&screensaverButton, "screensaverButton", ":/images/screensaver.svg", "Mouse dance"}
    };

    for (const auto& btn : buttons) {
        *btn.button = new QPushButton(cornerWidget);
        (*btn.button)->setObjectName(btn.objectName);
        setButtonIcon(*btn.button, btn.iconPath);
        (*btn.button)->setToolTip(tr(btn.tooltip));
    }
    
    screensaverButton->setCheckable(true);

    toggleSwitch->setFixedSize(78, 28);

    horizontalLayout->addWidget(keyboardLayoutComboBox);
    horizontalLayout->addWidget(screenScaleButton);
    horizontalLayout->addWidget(zoomInButton);
    horizontalLayout->addWidget(zoomOutButton);
    horizontalLayout->addWidget(zoomReductionButton);
    horizontalLayout->addWidget(virtualKeyboardButton);
    horizontalLayout->addWidget(captureButton);
    horizontalLayout->addWidget(fullScreenButton);
    horizontalLayout->addWidget(pasteButton);
    horizontalLayout->addWidget(screensaverButton);
    horizontalLayout->addWidget(toggleSwitch);
}

void CornerWidgetManager::setButtonIcon(QPushButton *button, const QString &iconPath)
{
    QIcon icon(iconPath);
    button->setIcon(icon);
    button->setIconSize(QSize(16, 16));
    button->setFixedSize(30, 30);
}

void CornerWidgetManager::setupConnections()
{
    connect(zoomInButton, &QPushButton::clicked, this, &CornerWidgetManager::zoomInClicked);
    connect(zoomOutButton, &QPushButton::clicked, this, &CornerWidgetManager::zoomOutClicked);
    connect(zoomReductionButton, &QPushButton::clicked, this, &CornerWidgetManager::zoomReductionClicked);
    connect(screenScaleButton, &QPushButton::clicked, this, &CornerWidgetManager::screenScaleClicked);
    connect(virtualKeyboardButton, &QPushButton::clicked, this, &CornerWidgetManager::virtualKeyboardClicked);
    connect(captureButton, &QPushButton::clicked, this, &CornerWidgetManager::captureClicked);
    connect(fullScreenButton, &QPushButton::clicked, this, &CornerWidgetManager::fullScreenClicked);
    connect(pasteButton, &QPushButton::clicked, this, &CornerWidgetManager::pasteClicked);
    connect(screensaverButton, &QPushButton::toggled, this, &CornerWidgetManager::screensaverClicked);
    connect(toggleSwitch, &ToggleSwitch::stateChanged, this, &CornerWidgetManager::toggleSwitchChanged);
    connect(keyboardLayoutComboBox, &QComboBox::currentTextChanged, this, &CornerWidgetManager::keyboardLayoutChanged);
}

void CornerWidgetManager::initializeKeyboardLayouts(const QStringList &layouts, const QString &defaultLayout)
{
    keyboardLayoutComboBox->clear();
    keyboardLayoutComboBox->addItems(layouts);
    if (layouts.contains(defaultLayout)) {
        keyboardLayoutComboBox->setCurrentText(defaultLayout);
    } else if (!layouts.isEmpty()) {
        keyboardLayoutComboBox->setCurrentText(layouts.first());
    }
}

void CornerWidgetManager::updateButtonVisibility(int windowWidth)
{
    qDebug() << "Entering updateButtonVisibility, windowWidth:" << windowWidth << ", layoutThreshold:" << layoutThreshold;

    // Define the step size for hiding buttons
    const int widthStep = 50; // Increased to make hiding more gradual

    // List of widgets in hiding order (first to hide is first in the list)
    QList<QWidget*> widgets = {
        screensaverButton,
        pasteButton,
        fullScreenButton,
        captureButton,
        virtualKeyboardButton,
        zoomReductionButton,
        zoomOutButton,
        zoomInButton,
        screenScaleButton,
        keyboardLayoutComboBox // Hidden last
        // toggleSwitch is excluded to ensure it remains visible
    };

    // Show all widgets by default
    for (QWidget* widget : widgets) {
        if (widget) {
            widget->show();
            qDebug() << "Initially showing widget:" << widget->objectName();
        }
    }
    toggleSwitch->show(); // Ensure toggleSwitch is always visible
    qDebug() << "Initially showing toggleSwitch";

    // Calculate how many widgets to hide based on window width
    int widgetsToHide = 0;
    if (windowWidth < layoutThreshold) {
        widgetsToHide = qMin(widgets.size(), (layoutThreshold - windowWidth) / widthStep);
    }
    qDebug() << "Widgets to hide:" << widgetsToHide;

    // Hide widgets based on calculated number
    for (int i = 0; i < widgetsToHide && i < widgets.size(); ++i) {
        if (widgets[i]) {
            widgets[i]->hide();
            qDebug() << "Hiding widget:" << widgets[i]->objectName() << "at index" << i
                     << "threshold:" << (layoutThreshold - (i * widthStep));
        }
    }

    // Update layout
    horizontalLayout->invalidate();
    horizontalLayout->activate();
    QSize layoutSizeHint = horizontalLayout->sizeHint();
    cornerWidget->setMinimumSize(layoutSizeHint);
    cornerWidget->resize(layoutSizeHint);
    cornerWidget->adjustSize();
    cornerWidget->show();

    if (cornerWidget->parentWidget()) {
        cornerWidget->parentWidget()->updateGeometry();
        cornerWidget->parentWidget()->update();
    }
    QApplication::processEvents();

    qDebug() << "Exiting updateButtonVisibility, cornerWidget geometry:" << cornerWidget->geometry()
             << ", layout sizeHint:" << layoutSizeHint;
    for (QWidget* widget : widgets) {
        if (widget) {
            qDebug() << "Widget" << widget->objectName() << "visible:" << widget->isVisible()
                     << "size:" << widget->size() << "pos:" << widget->pos()
                     << "geometry:" << widget->geometry();
        }
    }
    qDebug() << "Widget toggleSwitch visible:" << toggleSwitch->isVisible()
             << "size:" << toggleSwitch->size() << "pos:" << toggleSwitch->pos()
             << "geometry:" << toggleSwitch->geometry();
}

void CornerWidgetManager::updatePosition(int windowWidth, int menuBarHeight, bool isFullScreen)
{
    horizontalLayout->invalidate();
    horizontalLayout->activate();
    cornerWidget->setMinimumSize(horizontalLayout->sizeHint());
    cornerWidget->resize(horizontalLayout->sizeHint());
    cornerWidget->adjustSize();

    if (windowWidth < layoutThreshold || isFullScreen) {
        if (menuBar) {
            menuBar->setCornerWidget(nullptr, Qt::TopRightCorner);
        }
        cornerWidget->setParent(cornerWidget->parentWidget());

        QSize size = cornerWidget->size();
        int x = qMax(0, windowWidth - size.width() - 10);
        int y = isFullScreen ? 10 : (menuBarHeight > 0 ? menuBarHeight + 10 : 10);
        cornerWidget->setGeometry(x, y, size.width(), size.height());
        cornerWidget->raise();
        cornerWidget->show();
        qDebug() << "Floating position: (" << x << "," << y << "), size:" << size
                 << ", geometry:" << cornerWidget->geometry()
                 << ", layout sizeHint:" << horizontalLayout->sizeHint();
    } else {
        if (menuBar) {
            menuBar->setCornerWidget(cornerWidget, Qt::TopRightCorner);
            cornerWidget->show();
        }
        qDebug() << "Menu bar corner widget, size:" << cornerWidget->size()
                 << ", geometry:" << cornerWidget->geometry()
                 << ", layout sizeHint:" << horizontalLayout->sizeHint();
    }
}
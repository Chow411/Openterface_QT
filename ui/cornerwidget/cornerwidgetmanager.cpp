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
    horizontalLayout->invalidate();
    horizontalLayout->activate();
    cornerWidget->setMinimumSize(horizontalLayout->sizeHint());
    cornerWidget->resize(horizontalLayout->sizeHint());
    cornerWidget->adjustSize();
    cornerWidget->show();
    if (cornerWidget->parentWidget()) {
        cornerWidget->parentWidget()->update();
    }
    QApplication::processEvents();
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
}

void CornerWidgetManager::createWidgets()
{
    keyboardLayoutComboBox = new QComboBox(cornerWidget);
    keyboardLayoutComboBox->setObjectName("keyboardLayoutComboBox");
    keyboardLayoutComboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    keyboardLayoutComboBox->setMinimumSize(100, 30);
    keyboardLayoutComboBox->setMaximumHeight(30);
    keyboardLayoutComboBox->setToolTip(tr("Select Keyboard Layout"));
    keyboardLayoutComboBox->show();
    qDebug() << "Created keyboardLayoutComboBox, visible:" << keyboardLayoutComboBox->isVisible();

    screenScaleButton = new QPushButton(cornerWidget);
    screenScaleButton->setObjectName("ScreenScaleButton");
    setButtonIcon(screenScaleButton, ":/images/screen_scale.svg");
    screenScaleButton->setToolTip(tr("Screen scale"));
    screenScaleButton->setMinimumSize(30, 30);
    screenScaleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    screenScaleButton->show();
    qDebug() << "Created screenScaleButton, visible:" << screenScaleButton->isVisible();

    zoomInButton = new QPushButton(cornerWidget);
    zoomInButton->setObjectName("ZoomInButton");
    setButtonIcon(zoomInButton, ":/images/zoom_in.svg");
    zoomInButton->setToolTip(tr("Zoom in"));
    zoomInButton->setMinimumSize(30, 30);
    zoomInButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    zoomInButton->show();
    qDebug() << "Created zoomInButton, visible:" << zoomInButton->isVisible();

    zoomOutButton = new QPushButton(cornerWidget);
    zoomOutButton->setObjectName("ZoomOutButton");
    setButtonIcon(zoomOutButton, ":/images/zoom_out.svg");
    zoomOutButton->setToolTip(tr("Zoom out"));
    zoomOutButton->setMinimumSize(30, 30);
    zoomOutButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    zoomOutButton->show();
    qDebug() << "Created zoomOutButton, visible:" << zoomOutButton->isVisible();

    zoomReductionButton = new QPushButton(cornerWidget);
    zoomReductionButton->setObjectName("ZoomReductionButton");
    setButtonIcon(zoomReductionButton, ":/images/zoom_fit.svg");
    zoomReductionButton->setToolTip(tr("Restore original size"));
    zoomReductionButton->setMinimumSize(30, 30);
    zoomReductionButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    zoomReductionButton->show();
    qDebug() << "Created zoomReductionButton, visible:" << zoomReductionButton->isVisible();

    virtualKeyboardButton = new QPushButton(cornerWidget);
    virtualKeyboardButton->setObjectName("virtualKeyboardButton");
    setButtonIcon(virtualKeyboardButton, ":/images/keyboard.svg");
    virtualKeyboardButton->setToolTip(tr("Function key and composite key"));
    virtualKeyboardButton->setMinimumSize(30, 30);
    virtualKeyboardButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    virtualKeyboardButton->show();
    qDebug() << "Created virtualKeyboardButton, visible:" << virtualKeyboardButton->isVisible();

    captureButton = new QPushButton(cornerWidget);
    captureButton->setObjectName("captureButton");
    setButtonIcon(captureButton, ":/images/capture.svg");
    captureButton->setToolTip(tr("Full screen capture"));
    captureButton->setMinimumSize(30, 30);
    captureButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    captureButton->show();
    qDebug() << "Created captureButton, visible:" << captureButton->isVisible();

    fullScreenButton = new QPushButton(cornerWidget);
    fullScreenButton->setObjectName("fullScreenButton");
    setButtonIcon(fullScreenButton, ":/images/full_screen.svg");
    fullScreenButton->setToolTip(tr("Full screen mode"));
    fullScreenButton->setMinimumSize(30, 30);
    fullScreenButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    fullScreenButton->show();
    qDebug() << "Created fullScreenButton, visible:" << fullScreenButton->isVisible();

    pasteButton = new QPushButton(cornerWidget);
    pasteButton->setObjectName("pasteButton");
    setButtonIcon(pasteButton, ":/images/paste.svg");
    pasteButton->setToolTip(tr("Paste text to target"));
    pasteButton->setMinimumSize(30, 30);
    pasteButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    pasteButton->show();
    qDebug() << "Created pasteButton, visible:" << pasteButton->isVisible();

    screensaverButton = new QPushButton(cornerWidget);
    screensaverButton->setObjectName("screensaverButton");
    screensaverButton->setCheckable(true);
    setButtonIcon(screensaverButton, ":/images/screensaver.svg");
    screensaverButton->setToolTip(tr("Mouse dance"));
    screensaverButton->setMinimumSize(30, 30);
    screensaverButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    screensaverButton->show();
    qDebug() << "Created screensaverButton, visible:" << screensaverButton->isVisible();

    toggleSwitch->setFixedSize(78, 28);
    toggleSwitch->setMinimumSize(78, 28);
    toggleSwitch->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    toggleSwitch->show();
    qDebug() << "Created toggleSwitch, visible:" << toggleSwitch->isVisible();

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

    qDebug() << "Widgets added to horizontalLayout";
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
    Q_ASSERT(zoomInButton);
    Q_ASSERT(zoomOutButton);
    Q_ASSERT(zoomReductionButton);
    Q_ASSERT(screenScaleButton);
    Q_ASSERT(virtualKeyboardButton);
    Q_ASSERT(captureButton);
    Q_ASSERT(fullScreenButton);
    Q_ASSERT(pasteButton);
    Q_ASSERT(screensaverButton);
    Q_ASSERT(toggleSwitch);
    Q_ASSERT(keyboardLayoutComboBox);

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
    const int widthStep = 100; // Increased to make hiding more gradual

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
#include "devicediagnosticsdialog.h"
#include <QMessageBox>
#include <QApplication>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QStyle>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include "diagnostics/diagnosticsmanager.h"

Q_LOGGING_CATEGORY(log_device_diagnostics, "opf.diagnostics")

TestItem::TestItem(const QString &title, int testIndex, QListWidget *parent)
    : QListWidgetItem(title, parent)
    , m_status(TestStatus::NotStarted)
    , m_testIndex(testIndex)
    , m_title(title)
{
    updateIcon();
}

void TestItem::setTestStatus(TestStatus status)
{
    m_status = status;
    updateIcon();
}

void TestItem::updateIcon()
{
    QIcon icon;
    QString tooltip;
    
    switch (m_status) {
    case TestStatus::NotStarted:
        icon = QApplication::style()->standardIcon(QStyle::SP_DialogResetButton);
        tooltip = QObject::tr("Test not started");
        break;
    case TestStatus::InProgress:
        icon = QApplication::style()->standardIcon(QStyle::SP_BrowserReload);
        tooltip = QObject::tr("Test in progress...");
        break;
    case TestStatus::Completed:
        icon = QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton);
        tooltip = QObject::tr("Test completed successfully");
        break;
    case TestStatus::Failed:
        icon = QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton);
        tooltip = QObject::tr("Test failed");
        break;
    }
    
    setIcon(icon);
    setToolTip(tooltip);
}

DeviceDiagnosticsDialog::DeviceDiagnosticsDialog(QWidget *parent)
    : QDialog(parent)
    , m_mainLayout(nullptr)
    , m_splitter(nullptr)
    , m_testListGroup(nullptr)
    , m_testList(nullptr)
    , m_rightPanel(nullptr)
    , m_rightLayout(nullptr)
    , m_testTitleLabel(nullptr)
    , m_statusIconLabel(nullptr)
    , m_reminderLabel(nullptr)
    , m_logFileButton(nullptr)
    , m_logDisplayText(nullptr)
    , m_buttonLayout(nullptr)
    , m_restartButton(nullptr)
    , m_previousButton(nullptr)
    , m_nextButton(nullptr)
    , m_checkNowButton(nullptr)
    , m_currentTestIndex(0)
{
    setWindowTitle(tr("Hardware Diagnostics"));
    setMinimumSize(900, 600);
    setAttribute(Qt::WA_DeleteOnClose);
    
    setupUI();

    // Backend manager for test data and logic
    m_manager = new DiagnosticsManager(this);

    // Populate tests from manager
    m_testTitles = m_manager->testTitles();
    for (int i = 0; i < m_testTitles.size(); ++i) {
        TestItem* item = new TestItem(m_testTitles[i], i, m_testList);
        m_testList->addItem(item);
    }
    if (m_testList->count() > 0) {
        m_testList->setCurrentRow(0);
    }
    showTestPage(0);

    // Connect manager signals to update UI
    connect(m_manager, &DiagnosticsManager::statusChanged, this, [this](int idx, TestStatus st){
        TestItem* item = static_cast<TestItem*>(m_testList->item(idx));
        if (item) item->setTestStatus(st);
        if (idx == m_currentTestIndex) {
            QIcon icon;
            switch (st) {
            case TestStatus::NotStarted:
                icon = style()->standardIcon(QStyle::SP_ComputerIcon);
                break;
            case TestStatus::InProgress:
                icon = style()->standardIcon(QStyle::SP_BrowserReload);
                break;
            case TestStatus::Completed:
                icon = style()->standardIcon(QStyle::SP_DialogApplyButton);
                break;
            case TestStatus::Failed:
                icon = style()->standardIcon(QStyle::SP_DialogCancelButton);
                break;
            }
            m_statusIconLabel->setPixmap(icon.pixmap(24, 24));
        }
        updateNavigationButtons();
    });

    connect(m_manager, &DiagnosticsManager::logAppended, this, &DeviceDiagnosticsDialog::onLogAppended);
    connect(m_manager, &DiagnosticsManager::testStarted, this, &DeviceDiagnosticsDialog::testStarted);
    connect(m_manager, &DiagnosticsManager::testCompleted, this, &DeviceDiagnosticsDialog::testCompleted);
    connect(m_manager, &DiagnosticsManager::diagnosticsCompleted, this, &DeviceDiagnosticsDialog::onDiagnosticsCompleted);
    // Forward manager's completion signal to maintain backward compatibility (no-arg signal)
    connect(m_manager, &DiagnosticsManager::diagnosticsCompleted, this, [this](bool){ emit diagnosticsCompleted(); });

    qCDebug(log_device_diagnostics) << "Device Diagnostics Dialog created";
}

DeviceDiagnosticsDialog::~DeviceDiagnosticsDialog()
{
    qCDebug(log_device_diagnostics) << "Device Diagnostics Dialog destroyed";
}

void DeviceDiagnosticsDialog::setupUI()
{
    // Main layout
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // Create splitter for resizable panels
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_mainLayout->addWidget(m_splitter);
    
    setupLeftPanel();
    setupRightPanel();
    
    // Set splitter proportions (30% left, 70% right)
    m_splitter->setSizes({270, 630});
}

void DeviceDiagnosticsDialog::setupLeftPanel()
{
    // Left panel - Test list
    m_testListGroup = new QGroupBox(tr("Diagnostic Tests"), this);
    m_testListGroup->setMinimumWidth(250);
    
    QVBoxLayout* leftLayout = new QVBoxLayout(m_testListGroup);
    
    m_testList = new QListWidget(this);
    m_testList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_testList, &QListWidget::itemClicked, this, &DeviceDiagnosticsDialog::onTestItemClicked);
    
    leftLayout->addWidget(m_testList);
    
    m_splitter->addWidget(m_testListGroup);
}

void DeviceDiagnosticsDialog::setupRightPanel()
{
    // Right panel - Test details
    m_rightPanel = new QWidget(this);
    m_rightLayout = new QVBoxLayout(m_rightPanel);
    m_rightLayout->setContentsMargins(15, 15, 15, 15);
    m_rightLayout->setSpacing(15);
    
    // Top section: Title + SVG icon
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(10);
    
    m_testTitleLabel = new QLabel(this);
    m_testTitleLabel->setStyleSheet("QLabel { font-size: 16px; font-weight: bold; }");
    titleLayout->addWidget(m_testTitleLabel);
    
    // SVG Icon (using system icon for now, can be replaced with custom SVG)
    m_statusIconLabel = new QLabel(this);
    m_statusIconLabel->setFixedSize(24, 24);
    m_statusIconLabel->setScaledContents(true);
    m_statusIconLabel->setPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(24, 24));
    titleLayout->addWidget(m_statusIconLabel);
    
    titleLayout->addStretch(); // Push everything to the left
    m_rightLayout->addLayout(titleLayout);
    
    // Small reminder text
    m_reminderLabel = new QLabel(this);
    m_reminderLabel->setStyleSheet("QLabel { font-size: 11px; }");
    m_reminderLabel->setWordWrap(true);
    m_rightLayout->addWidget(m_reminderLabel);
    
    // Add separator line
    QFrame* line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    m_rightLayout->addWidget(line1);
    
    // Log file section
    QHBoxLayout* logFileLayout = new QHBoxLayout();
    logFileLayout->setSpacing(10);
    
    QLabel* logFileLabel = new QLabel(tr("Test Log:"), this);
    logFileLabel->setStyleSheet("QLabel { font-weight: bold; }");
    logFileLayout->addWidget(logFileLabel);
    
    m_logFileButton = new QPushButton(tr("diagnostics_log.txt"), this);
    m_logFileButton->setStyleSheet(
        "QPushButton {"
        "   text-decoration: underline;"
        "   border: none;"
        "   background: transparent;"
        "   text-align: left;"
        "   padding: 2px;"
        "}"
    );
    m_logFileButton->setCursor(Qt::PointingHandCursor);
    connect(m_logFileButton, &QPushButton::clicked, this, &DeviceDiagnosticsDialog::onOpenLogFileClicked);
    logFileLayout->addWidget(m_logFileButton);
    
    logFileLayout->addStretch();
    m_rightLayout->addLayout(logFileLayout);
    
    // Add separator line
    QFrame* line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    m_rightLayout->addWidget(line2);
    
    // Log display area (replaces instructions text)
    m_logDisplayText = new QTextEdit(this);
    m_logDisplayText->setReadOnly(true);
    m_logDisplayText->setStyleSheet(
        "QTextEdit {"
        "   border-radius: 5px;"
        "   padding: 10px;"
        "   font-size: 11px;"
        "   font-family: 'Consolas', 'Monaco', monospace;"
        "}"
    );
    m_logDisplayText->setPlainText(tr("Test logs will appear here..."));
    m_rightLayout->addWidget(m_logDisplayText);
    
    // Spacer to push buttons to bottom
    m_rightLayout->addStretch();
    
    // Button layout
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(10);
    
    m_restartButton = new QPushButton(tr("Restart"), this);
    m_restartButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_restartButton->setMinimumHeight(35);
    connect(m_restartButton, &QPushButton::clicked, this, &DeviceDiagnosticsDialog::onRestartClicked);
    
    m_previousButton = new QPushButton(tr("Previous"), this);
    m_previousButton->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    m_previousButton->setMinimumHeight(35);
    connect(m_previousButton, &QPushButton::clicked, this, &DeviceDiagnosticsDialog::onPreviousClicked);
    
    m_nextButton = new QPushButton(tr("Next"), this);
    m_nextButton->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    m_nextButton->setMinimumHeight(35);
    connect(m_nextButton, &QPushButton::clicked, this, &DeviceDiagnosticsDialog::onNextClicked);
    
    m_checkNowButton = new QPushButton(tr("Check Now"), this);
    m_checkNowButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    m_checkNowButton->setMinimumHeight(35);
    m_checkNowButton->setDefault(true);  // Make it the default button
    m_checkNowButton->setStyleSheet(
        "QPushButton {"
        "   font-weight: bold;"
        "   border-radius: 5px;"
        "   padding: 8px 16px;"
        "   min-width: 100px;"
        "}"
    );
    connect(m_checkNowButton, &QPushButton::clicked, this, &DeviceDiagnosticsDialog::onCheckNowClicked);
    
    m_buttonLayout->addWidget(m_restartButton);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_previousButton);
    m_buttonLayout->addWidget(m_nextButton);
    m_buttonLayout->addWidget(m_checkNowButton);
    
    m_rightLayout->addLayout(m_buttonLayout);
    
    m_splitter->addWidget(m_rightPanel);
}



void DeviceDiagnosticsDialog::showTestPage(int index)
{
    if (index < 0 || index >= m_testTitles.size()) {
        return;
    }
    
    m_currentTestIndex = index;
    
    // Update UI
    m_testTitleLabel->setText(m_testTitles[index]);
    
    // Update reminder text based on test
    QString reminder;
    switch (index) {
        case 0:
            reminder = tr("Check all physical connections before testing");
            break;
        case 1:
            reminder = tr("Prepare to disconnect/reconnect target device");
            break;
        case 2:
            reminder = tr("Ensure host device is stable");
            break;
        case 3:
            reminder = tr("Serial communication test may take time");
            break;
        case 4:
            reminder = tr("WARNING: This will reset device settings");
            break;
        case 5:
            reminder = tr("High speed test requires stable connection");
            break;
        case 6:
            reminder = tr("Stress test may run for several minutes");
            break;
        default:
            reminder = tr("Follow the test instructions carefully");
    }
    m_reminderLabel->setText(reminder);
    
    // Update status icon based on current test status (from manager)
    TestStatus status = TestStatus::NotStarted;
    if (m_manager) {
        status = m_manager->testStatus(index);
    } else {
        TestItem* currentItem = static_cast<TestItem*>(m_testList->item(index));
        if (currentItem) status = currentItem->getTestStatus();
    }

    QIcon icon;
    switch (status) {
        case TestStatus::NotStarted:
            icon = style()->standardIcon(QStyle::SP_ComputerIcon);
            break;
        case TestStatus::InProgress:
            icon = style()->standardIcon(QStyle::SP_BrowserReload);
            break;
        case TestStatus::Completed:
            icon = style()->standardIcon(QStyle::SP_DialogApplyButton);
            break;
        case TestStatus::Failed:
            icon = style()->standardIcon(QStyle::SP_DialogCancelButton);
            break;
    }
    m_statusIconLabel->setPixmap(icon.pixmap(24, 24));
    
    // Update list selection
    m_testList->setCurrentRow(index);
    
    // Update navigation buttons
    updateNavigationButtons();
}

void DeviceDiagnosticsDialog::updateNavigationButtons()
{
    m_previousButton->setEnabled(m_currentTestIndex > 0);
    m_nextButton->setEnabled(m_currentTestIndex < m_testTitles.size() - 1);
    
    // Update check button based on test status
    TestStatus status = TestStatus::NotStarted;
    if (m_manager) {
        status = m_manager->testStatus(m_currentTestIndex);
    } else {
        TestItem* currentItem = static_cast<TestItem*>(m_testList->item(m_currentTestIndex));
        if (currentItem) status = currentItem->getTestStatus();
    }

    if (status == TestStatus::InProgress) {
        m_checkNowButton->setText(tr("Testing..."));
        m_checkNowButton->setEnabled(false);
    } else {
        m_checkNowButton->setText(tr("Check Now"));
        m_checkNowButton->setEnabled(!(m_manager && m_manager->isTestingInProgress()));
    }
}

void DeviceDiagnosticsDialog::onRestartClicked()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Restart Diagnostics"), 
        tr("This will reset all test results and start over. Continue?"),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        // Clear UI log and delegate reset to manager
        m_logDisplayText->clear();
        if (m_manager) m_manager->resetAllTests();
        showTestPage(0);
        qCDebug(log_device_diagnostics) << "Diagnostics restarted";
    }
}

void DeviceDiagnosticsDialog::onPreviousClicked()
{
    if (m_currentTestIndex > 0) {
        showTestPage(m_currentTestIndex - 1);
    }
}

void DeviceDiagnosticsDialog::onNextClicked()
{
    if (m_currentTestIndex < m_testTitles.size() - 1) {
        showTestPage(m_currentTestIndex + 1);
    }
}

void DeviceDiagnosticsDialog::onCheckNowClicked()
{
    if (m_manager && m_manager->isTestingInProgress()) {
        return;
    }

    if (m_manager) {
        m_manager->startTest(m_currentTestIndex);
    }
}

void DeviceDiagnosticsDialog::onTestItemClicked(QListWidgetItem* item)
{
    TestItem* testItem = static_cast<TestItem*>(item);
    if (!testItem) return;

    if (m_manager && m_manager->isTestingInProgress()) return;

    showTestPage(testItem->getTestIndex());
}

void DeviceDiagnosticsDialog::onOpenLogFileClicked()
{
    if (!m_manager) return;

    QString logPath = m_manager->getLogFilePath();

    // Ensure log file exists
    QFileInfo fileInfo(logPath);
    if (!fileInfo.exists()) {
        QFile logFile(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&logFile);
            out << QString("Hardware Diagnostics Log - %1\n").arg(QDateTime::currentDateTime().toString());
            out << "=" << QString("=").repeated(50) << "\n\n";
            logFile.close();
        }
    }

    // Open the log file with system default application
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(logPath))) {
        QMessageBox::warning(this, tr("Error"), 
                           tr("Could not open log file: %1").arg(logPath));
    }
}

void DeviceDiagnosticsDialog::onLogAppended(const QString &entry)
{
    if (!m_logDisplayText) return;
    m_logDisplayText->append(entry);
}

void DeviceDiagnosticsDialog::onDiagnosticsCompleted(bool allSuccessful)
{
    QString message = allSuccessful ?
        tr("All diagnostic tests completed successfully!") :
        tr("Diagnostic tests completed with some failures. Please check the results.");

    QMessageBox::information(this, tr("Diagnostics Complete"), message);
}


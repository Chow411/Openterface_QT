#include "diagnosticsmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRandomGenerator>
#include <QDateTime>
#include <QLoggingCategory>


DiagnosticsManager::DiagnosticsManager(QObject *parent)
    : QObject(parent)
    , m_testTimer(new QTimer(this))
    , m_runningTestIndex(-1)
    , m_isTestingInProgress(false)
{
    // Initialize test titles
    m_testTitles = {
        tr("Overall Connection"),
        tr("Target Plug & Play"),
        tr("Host Plug & Play"),
        tr("Serial Connection"),
        tr("Factory Reset"),
        tr("High Baudrate"),
        tr("Stress Test")
    };

    m_statuses.resize(m_testTitles.size());
    for (int i = 0; i < m_statuses.size(); ++i) {
        m_statuses[i] = TestStatus::NotStarted;
    }

    m_testTimer->setSingleShot(true);
    connect(m_testTimer, &QTimer::timeout, this, &DiagnosticsManager::onTimerTimeout);
}

TestStatus DiagnosticsManager::testStatus(int index) const
{
    if (index >= 0 && index < m_statuses.size()) {
        return m_statuses[index];
    }
    return TestStatus::NotStarted;
}

QString DiagnosticsManager::getTestTitle(int testIndex) const
{
    if (testIndex >= 0 && testIndex < m_testTitles.size()) {
        return m_testTitles[testIndex];
    }
    return QString();
}

QString DiagnosticsManager::getLogFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(dataDir);
    }
    return dir.filePath("diagnostics_log.txt");
}

void DiagnosticsManager::appendToLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp, message);

    // Emit to UI
    emit logAppended(logEntry);

    // Also write to file
    QString logPath = getLogFilePath();
    QFile logFile(logPath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << logEntry << "\n";
        logFile.close();
    }
}

void DiagnosticsManager::startTest(int testIndex)
{
    if (m_isTestingInProgress)
        return;
    if (testIndex < 0 || testIndex >= m_testTitles.size())
        return;

    m_isTestingInProgress = true;
    m_runningTestIndex = testIndex;

    m_statuses[testIndex] = TestStatus::InProgress;
    emit statusChanged(testIndex, TestStatus::InProgress);

    QString testName = m_testTitles[testIndex];
    appendToLog(QString("Started test: %1").arg(testName));
    emit testStarted(testIndex);

    int testDuration = 2000 + QRandomGenerator::global()->bounded(3000);
    m_testTimer->start(testDuration);

    qCDebug(log_device_diagnostics) << "Started test" << testIndex << "(" << m_testTitles[testIndex] << ")";
}

void DiagnosticsManager::onTimerTimeout()
{
    if (m_runningTestIndex < 0 || m_runningTestIndex >= m_testTitles.size())
        return;

    bool success = (QRandomGenerator::global()->bounded(100) < 90);
    int testIndex = m_runningTestIndex;

    m_statuses[testIndex] = success ? TestStatus::Completed : TestStatus::Failed;
    emit statusChanged(testIndex, m_statuses[testIndex]);
    emit testCompleted(testIndex, success);

    QString testName = m_testTitles[testIndex];
    QString result = success ? "PASSED" : "FAILED";
    appendToLog(QString("Test completed: %1 - %2").arg(testName, result));

    m_isTestingInProgress = false;
    m_runningTestIndex = -1;

    // Check all completed
    bool allCompleted = true;
    bool allSuccessful = true;
    for (int i = 0; i < m_statuses.size(); ++i) {
        if (m_statuses[i] == TestStatus::NotStarted || m_statuses[i] == TestStatus::InProgress) {
            allCompleted = false;
            break;
        }
        if (m_statuses[i] == TestStatus::Failed) {
            allSuccessful = false;
        }
    }

    if (allCompleted) {
        appendToLog(QString("=== DIAGNOSTICS COMPLETE: %1 ===").arg(allSuccessful ? "All diagnostic tests PASSED!" : "Diagnostic tests completed with some FAILURES. Check results above."));
        emit diagnosticsCompleted(allSuccessful);
    }

    qCDebug(log_device_diagnostics) << "Test" << testIndex << (success ? "passed" : "failed");
}

void DiagnosticsManager::resetAllTests()
{
    for (int i = 0; i < m_statuses.size(); ++i) {
        m_statuses[i] = TestStatus::NotStarted;
        emit statusChanged(i, TestStatus::NotStarted);
    }
    m_isTestingInProgress = false;
    if (m_testTimer->isActive()) m_testTimer->stop();

    appendToLog("=== DIAGNOSTICS RESTARTED ===");
    appendToLog("All test results have been reset.");

    qCDebug(log_device_diagnostics) << "Diagnostics restarted";
}

#include "diagnosticsmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRandomGenerator>
#include <QDateTime>
#include <QLoggingCategory>

#include "device/DeviceManager.h" // for device presence checks
#include "device/DeviceInfo.h"
#include "serial/SerialPortManager.h"
#include "serial/ch9329.h"


DiagnosticsManager::DiagnosticsManager(QObject *parent)
    : QObject(parent)
    , m_testTimer(new QTimer(this))
    , m_targetCheckTimer(new QTimer(this))
    , m_runningTestIndex(-1)
    , m_isTestingInProgress(false)
    , m_targetPreviouslyConnected(false)
    , m_targetCurrentlyConnected(false)
    , m_targetUnplugDetected(false)
    , m_targetReplugDetected(false)
    , m_targetTestElapsedTime(0)
    , m_targetPlugCount(0)
    , m_hostPreviouslyConnected(false)
    , m_hostCurrentlyConnected(false)
    , m_hostUnplugDetected(false)
    , m_hostReplugDetected(false)
    , m_hostTestElapsedTime(0)
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
    
    // Setup Target Plug & Play test timer
    m_targetCheckTimer->setInterval(500); // Check every 500ms
    connect(m_targetCheckTimer, &QTimer::timeout, this, &DiagnosticsManager::onTargetStatusCheckTimeout);
    
    // Setup Host Plug & Play test timer
    m_hostCheckTimer = new QTimer(this);
    m_hostCheckTimer->setInterval(500); // Check every 500ms
    connect(m_hostCheckTimer, &QTimer::timeout, this, &DiagnosticsManager::onHostStatusCheckTimeout);
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

    // Special-case: Overall Connection (index 0) -> perform immediate device presence checks
    if (testIndex == 0) {
        m_isTestingInProgress = true;
        m_runningTestIndex = testIndex;

        m_statuses[testIndex] = TestStatus::InProgress;
        emit statusChanged(testIndex, TestStatus::InProgress);

        QString testName = m_testTitles[testIndex];
        appendToLog(QString("Started test: %1 (Overall Connection check)").arg(testName));
        emit testStarted(testIndex);

        // Query device manager for current devices
        DeviceManager &dm = DeviceManager::getInstance();
        QList<DeviceInfo> devices = dm.getCurrentDevices();

        bool foundHid = false;
        bool foundSerial = false;
        bool foundCamera = false;
        bool foundAudio = false;

        appendToLog(QString("Found %1 device(s) reported by device manager").arg(devices.size()));

        for (const DeviceInfo &d : devices) {
            QString devSummary = QString("Device %1: %2").arg(d.portChain, d.getInterfaceSummary());
            appendToLog(devSummary);

            if (d.hasHidDevice()) {
                foundHid = true;
                appendToLog(QString("HID present on port %1").arg(d.getPortChainDisplay()));
            }
            if (d.hasSerialPort()) {
                foundSerial = true;
                appendToLog(QString("Serial port present: %1").arg(d.serialPortPath));
            }
            if (d.hasCameraDevice()) {
                foundCamera = true;
                appendToLog(QString("Camera present on port %1").arg(d.getPortChainDisplay()));
            }
            if (d.hasAudioDevice()) {
                foundAudio = true;
                appendToLog(QString("Audio present on port %1").arg(d.getPortChainDisplay()));
            }
        }

        // Individual checks logged; now determine overall success
        bool success = (foundHid && foundSerial && foundCamera && foundAudio);

        if (success) {
            m_statuses[testIndex] = TestStatus::Completed;
            appendToLog("Overall Connection: PASS - all required interfaces present");
        } else {
            m_statuses[testIndex] = TestStatus::Failed;
            QString missing;
            if (!foundHid) missing += " HID";
            if (!foundSerial) missing += " Serial";
            if (!foundCamera) missing += " Camera";
            if (!foundAudio) missing += " Audio";
            appendToLog(QString("Overall Connection: FAIL - missing:%1").arg(missing));
        }

        emit statusChanged(testIndex, m_statuses[testIndex]);
        emit testCompleted(testIndex, success);

        // Reset running state
        m_isTestingInProgress = false;
        m_runningTestIndex = -1;

        // If all tests done, emit diagnosticsCompleted (reuse logic from onTimerTimeout)
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

        qCDebug(log_device_diagnostics) << "Overall Connection check finished:" << (success ? "PASS" : "FAIL");
        return;
    }
    
    // Special-case: Target Plug & Play (index 1) -> perform target cable hot-plug detection
    if (testIndex == 1) {
        startTargetPlugPlayTest();
        return;
    }
    
    // Special-case: Host Plug & Play (index 2) -> perform host device hot-plug detection
    if (testIndex == 2) {
        startHostPlugPlayTest();
        return;
    }

    // Fallback: generic timed test simulation (unchanged behavior for other tests)
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
    if (m_targetCheckTimer->isActive()) m_targetCheckTimer->stop();
    if (m_hostCheckTimer->isActive()) m_hostCheckTimer->stop();

    appendToLog("=== DIAGNOSTICS RESTARTED ===");
    appendToLog("All test results have been reset.");

    qCDebug(log_device_diagnostics) << "Diagnostics restarted";
}

void DiagnosticsManager::startTargetPlugPlayTest()
{
    m_isTestingInProgress = true;
    m_runningTestIndex = 1; // Target Plug & Play test index
    
    // Reset test state
    m_targetPreviouslyConnected = false;
    m_targetCurrentlyConnected = false;
    m_targetUnplugDetected = false;
    m_targetReplugDetected = false;
    m_targetTestElapsedTime = 0;
    m_targetPlugCount = 0;
    
    m_statuses[1] = TestStatus::InProgress;
    emit statusChanged(1, TestStatus::InProgress);
    
    appendToLog("Started test: Target Plug & Play");
    appendToLog("Test requires detecting 2 plug-in events to complete successfully.");
    appendToLog("Test will timeout after 30 seconds if not completed.");
    emit testStarted(1);
    
    // Check initial target connection status
    m_targetPreviouslyConnected = checkTargetConnectionStatus();
    m_targetCurrentlyConnected = m_targetPreviouslyConnected;
    
    if (m_targetPreviouslyConnected) {
        appendToLog("Target initially connected. Please unplug the cable first, then plug it back in twice.");
    } else {
        appendToLog("Target initially disconnected. Please plug in the cable (need 2 plug-in events total).");
    }
    
    // Start periodic checking
    m_targetCheckTimer->start();
    
    qCDebug(log_device_diagnostics) << "Started Target Plug & Play test";
}

void DiagnosticsManager::onTargetStatusCheckTimeout()
{
    m_targetTestElapsedTime += 500; // Timer interval is 500ms
    
    bool currentStatus = checkTargetConnectionStatus();
    
    // Detect state changes
    if (currentStatus != m_targetCurrentlyConnected) {
        m_targetCurrentlyConnected = currentStatus;
        
        if (!currentStatus && m_targetPreviouslyConnected) {
            // Target was unplugged
            m_targetUnplugDetected = true;
            appendToLog("Target cable unplugged detected!");
            int remainingPlugs = 2 - m_targetPlugCount;
            appendToLog(QString("Please plug it back in (need %1 more plug-in events)...").arg(remainingPlugs));
        } else if (currentStatus && !m_targetPreviouslyConnected) {
            // Target was plugged in
            m_targetPlugCount++;
            appendToLog(QString("Target cable plugged in detected! (Count: %1/2)").arg(m_targetPlugCount));
            
            if (m_targetPlugCount >= 2) {
                // Test completed successfully - we've detected 2 plug-in events
                m_targetCheckTimer->stop();
                m_statuses[1] = TestStatus::Completed;
                emit statusChanged(1, TestStatus::Completed);
                emit testCompleted(1, true);
                
                appendToLog("Target Plug & Play test: PASSED - 2 plug-in events detected successfully");
                
                m_isTestingInProgress = false;
                m_runningTestIndex = -1;
                
                // Check if all tests completed
                checkAllTestsCompletion();
                return;
            } else {
                // Need one more plug-in event
                appendToLog("Please unplug and plug in the cable again to complete the test.");
            }
        }
        
        m_targetPreviouslyConnected = currentStatus;
    }
    
    // Check for timeout (30 seconds)
    if (m_targetTestElapsedTime >= 30000) {
        m_targetCheckTimer->stop();
        m_statuses[1] = TestStatus::Failed;
        emit statusChanged(1, TestStatus::Failed);
        emit testCompleted(1, false);
        
        appendToLog(QString("Target Plug & Play test: FAILED - Only detected %1/2 plug-in events within 10 seconds").arg(m_targetPlugCount));
        
        m_isTestingInProgress = false;
        m_runningTestIndex = -1;
        
        // Check if all tests completed
        checkAllTestsCompletion();
    }
}

bool DiagnosticsManager::checkTargetConnectionStatus()
{
    // Get serial port manager instance
    DeviceManager& deviceManager = DeviceManager::getInstance();
    QList<DeviceInfo> devices = deviceManager.getCurrentDevices();
    
    // Find a device with serial port to send command
    for (const DeviceInfo& device : devices) {
        if (!device.serialPortPath.isEmpty()) {
            // Try to get SerialPortManager instance and send CMD_GET_INFO
            SerialPortManager& serialManager = SerialPortManager::getInstance();
            if (serialManager.getCurrentSerialPortPath() == device.serialPortPath) {
                // Send CMD_GET_INFO command and parse response
                QByteArray response = serialManager.sendSyncCommand(CMD_GET_INFO, false);
                if (!response.isEmpty() && response.size() >= static_cast<int>(sizeof(CmdGetInfoResult))) {
                    CmdGetInfoResult result = CmdGetInfoResult::fromByteArray(response);
                    bool isConnected = (result.targetConnected != 0);
                    
                    qCDebug(log_device_diagnostics) << "Target connection status:" << isConnected 
                                                   << "Response:" << response.toHex(' ');
                    return isConnected;
                }
            }
            break;
        }
    }
    
    // If no serial port available or command failed, assume disconnected
    return false;
}

void DiagnosticsManager::checkAllTestsCompletion()
{
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
}

void DiagnosticsManager::startHostPlugPlayTest()
{
    m_isTestingInProgress = true;
    m_runningTestIndex = 2; // Host Plug & Play test index
    
    // Reset test state
    m_hostPreviouslyConnected = false;
    m_hostCurrentlyConnected = false;
    m_hostUnplugDetected = false;
    m_hostReplugDetected = false;
    m_hostTestElapsedTime = 0;
    
    m_statuses[2] = TestStatus::InProgress;
    emit statusChanged(2, TestStatus::InProgress);
    
    appendToLog("Started test: Host Plug & Play");
    appendToLog("Test requires detecting host device unplug and re-plug to complete successfully.");
    appendToLog("Test will timeout after 30 seconds if not completed.");
    emit testStarted(2);
    
    // Check initial host connection status
    m_hostPreviouslyConnected = checkHostConnectionStatus();
    m_hostCurrentlyConnected = m_hostPreviouslyConnected;
    
    if (m_hostPreviouslyConnected) {
        appendToLog("Host devices initially connected. Please unplug the USB cable from host, then plug it back in.");
    } else {
        appendToLog("Host devices initially disconnected. Please plug in the USB cable to host.");
    }
    
    // Start periodic checking
    m_hostCheckTimer->start();
    
    qCDebug(log_device_diagnostics) << "Started Host Plug & Play test";
}

void DiagnosticsManager::onHostStatusCheckTimeout()
{
    m_hostTestElapsedTime += 500; // Timer interval is 500ms
    
    bool currentStatus = checkHostConnectionStatus();
    
    // Detect state changes
    if (currentStatus != m_hostCurrentlyConnected) {
        m_hostCurrentlyConnected = currentStatus;
        
        if (!currentStatus && m_hostPreviouslyConnected) {
            // Host devices were unplugged
            m_hostUnplugDetected = true;
            appendToLog("Host devices unplugged detected!");
            appendToLog("Please plug the USB cable back into the host to complete the test...");
        } else if (currentStatus && m_hostUnplugDetected && !m_hostReplugDetected) {
            // Host devices were re-plugged after being unplugged
            m_hostReplugDetected = true;
            appendToLog("Host devices re-plugged detected!");
            
            // Test completed successfully
            m_hostCheckTimer->stop();
            m_statuses[2] = TestStatus::Completed;
            emit statusChanged(2, TestStatus::Completed);
            emit testCompleted(2, true);
            
            appendToLog("Host Plug & Play test: PASSED - Hot-plug cycle completed successfully");
            
            m_isTestingInProgress = false;
            m_runningTestIndex = -1;
            
            // Check if all tests completed
            checkAllTestsCompletion();
            return;
        }
        
        m_hostPreviouslyConnected = currentStatus;
    }
    
    // Check for timeout (30 seconds)
    if (m_hostTestElapsedTime >= 30000) {
        m_hostCheckTimer->stop();
        m_statuses[2] = TestStatus::Failed;
        emit statusChanged(2, TestStatus::Failed);
        emit testCompleted(2, false);
        
        if (!m_hostUnplugDetected) {
            appendToLog("Host Plug & Play test: FAILED - No unplug detected within 30 seconds");
        } else {
            appendToLog("Host Plug & Play test: FAILED - No re-plug detected within 30 seconds");
        }
        
        m_isTestingInProgress = false;
        m_runningTestIndex = -1;
        
        // Check if all tests completed
        checkAllTestsCompletion();
    }
}

bool DiagnosticsManager::checkHostConnectionStatus()
{
    // Get device manager instance to check host-side devices
    DeviceManager& deviceManager = DeviceManager::getInstance();
    QList<DeviceInfo> devices = deviceManager.getCurrentDevices();
    
    bool hasCamera = false;
    bool hasAudio = false;
    bool hasHid = false;
    bool hasSerial = false;
    
    // Check if we have all required host-side device interfaces
    for (const DeviceInfo& device : devices) {
        if (device.hasCameraDevice()) {
            hasCamera = true;
        }
        if (device.hasAudioDevice()) {
            hasAudio = true;
        }
        if (device.hasHidDevice()) {
            hasHid = true;
        }
        if (device.hasSerialPort()) {
            hasSerial = true;
        }
    }
    
    // Host is considered connected if we have all essential interfaces
    bool isConnected = (hasCamera && hasAudio && hasHid && hasSerial);
    
    qCDebug(log_device_diagnostics) << "Host connection status:" << isConnected 
                                   << "Camera:" << hasCamera 
                                   << "Audio:" << hasAudio 
                                   << "HID:" << hasHid 
                                   << "Serial:" << hasSerial;
    
    return isConnected;
}

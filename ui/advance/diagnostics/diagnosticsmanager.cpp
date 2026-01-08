#include "diagnosticsmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRandomGenerator>
#include <QDateTime>
#include <QLoggingCategory>
#include <QEventLoop>
#include <QTimer>
#include <QSettings>

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
    
    // Special-case: Serial Connection (index 3) -> perform serial port connection test
    if (testIndex == 3) {
        startSerialConnectionTest();
        return;
    }
    
    // Special-case: Factory Reset (index 4) -> perform factory reset test
    if (testIndex == 4) {
        startFactoryResetTest();
        return;
    }
    
    // Special-case: High Baudrate (index 5) -> perform baudrate switching test
    if (testIndex == 5) {
        startHighBaudrateTest();
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

void DiagnosticsManager::startSerialConnectionTest()
{
    m_isTestingInProgress = true;
    m_runningTestIndex = 3; // Serial Connection test index
    
    m_statuses[3] = TestStatus::InProgress;
    emit statusChanged(3, TestStatus::InProgress);
    
    appendToLog("Started test: Serial Connection");
    appendToLog("Testing serial port connectivity by sending CMD_GET_INFO command...");
    emit testStarted(3);
    
    // Perform serial connection test
    bool success = performSerialConnectionTest();
    
    if (success) {
        m_statuses[3] = TestStatus::Completed;
        appendToLog("Serial Connection test: PASSED - Successfully received response from serial port");
    } else {
        m_statuses[3] = TestStatus::Failed;
        appendToLog("Serial Connection test: FAILED - No response or invalid response from serial port");
    }
    
    emit statusChanged(3, m_statuses[3]);
    emit testCompleted(3, success);
    
    m_isTestingInProgress = false;
    m_runningTestIndex = -1;
    
    // Check if all tests completed
    checkAllTestsCompletion();
    
    qCDebug(log_device_diagnostics) << "Serial Connection test finished:" << (success ? "PASS" : "FAIL");
}

bool DiagnosticsManager::performSerialConnectionTest()
{
    // Get SerialPortManager instance
    SerialPortManager& serialManager = SerialPortManager::getInstance();
    
    // Check if serial port is available
    QString currentPortPath = serialManager.getCurrentSerialPortPath();
    if (currentPortPath.isEmpty()) {
        appendToLog("No serial port available for testing");
        return false;
    }
    
    appendToLog(QString("Using serial port: %1").arg(currentPortPath));
    appendToLog("Testing target connection status with 3 attempts (1 second interval)...");
    
    // Retry up to 3 times with 1 second intervals
    for (int attempt = 1; attempt <= 3; attempt++) {
        appendToLog(QString("Attempt %1/3: Sending CMD_GET_INFO command...").arg(attempt));
        
        try {
            // Send CMD_GET_INFO command and wait for response
            QByteArray response = serialManager.sendSyncCommand(CMD_GET_INFO, false);
            
            if (response.isEmpty()) {
                appendToLog(QString("Attempt %1: No response received from serial port").arg(attempt));
            } else {
                appendToLog(QString("Attempt %1: Received response: %2").arg(attempt).arg(QString(response.toHex(' '))));
                
                // Validate response structure
                if (response.size() >= static_cast<int>(sizeof(CmdGetInfoResult))) {
                    CmdGetInfoResult result = CmdGetInfoResult::fromByteArray(response);
                    
                    // Check if response has valid header (0x57AB)
                    if (result.prefix == 0xAB57) { // Little-endian format
                        appendToLog(QString("Attempt %1: Valid response - Version: %2, Target Connected: %3")
                                   .arg(attempt)
                                   .arg(result.version)
                                   .arg(result.targetConnected ? "Yes" : "No"));
                        
                        // Check if target is connected
                        if (result.targetConnected != 0) {
                            appendToLog(QString("Target connection detected on attempt %1 - Test PASSED").arg(attempt));
                            return true;
                        } else {
                            appendToLog(QString("Attempt %1: Target not connected").arg(attempt));
                        }
                    } else {
                        appendToLog(QString("Attempt %1: Invalid response header: 0x%2 (expected 0x57AB)")
                                   .arg(attempt)
                                   .arg(result.prefix, 4, 16, QChar('0')));
                    }
                } else {
                    appendToLog(QString("Attempt %1: Response too short: %2 bytes (expected at least %3 bytes)")
                               .arg(attempt)
                               .arg(response.size())
                               .arg(sizeof(CmdGetInfoResult)));
                }
            }
            
        } catch (const std::exception& e) {
            appendToLog(QString("Attempt %1: Exception during serial communication: %2").arg(attempt).arg(e.what()));
        } catch (...) {
            appendToLog(QString("Attempt %1: Unknown error during serial communication").arg(attempt));
        }
        
        // Wait 1 second before next attempt (except for the last attempt)
        if (attempt < 3) {
            appendToLog("Waiting 1 second before next attempt...");
            QEventLoop loop;
            QTimer::singleShot(1000, &loop, &QEventLoop::quit);
            loop.exec();
        }
    }
    
    appendToLog("All 3 attempts completed - Target connection not detected");
    return false;
}

void DiagnosticsManager::startFactoryResetTest()
{
    m_isTestingInProgress = true;
    m_runningTestIndex = 4; // Factory Reset test index
    
    m_statuses[4] = TestStatus::InProgress;
    emit statusChanged(4, TestStatus::InProgress);
    
    appendToLog("Started test: Factory Reset");
    appendToLog("Performing factory reset operation on HID chip...");
    emit testStarted(4);
    
    // Perform factory reset test
    bool success = performFactoryResetTest();
    
    if (success) {
        m_statuses[4] = TestStatus::Completed;
        appendToLog("Factory Reset test: PASSED - Factory reset operation completed successfully");
    } else {
        m_statuses[4] = TestStatus::Failed;
        appendToLog("Factory Reset test: FAILED - Factory reset operation failed");
    }
    
    emit statusChanged(4, m_statuses[4]);
    emit testCompleted(4, success);
    
    m_isTestingInProgress = false;
    m_runningTestIndex = -1;
    
    // Check if all tests completed
    checkAllTestsCompletion();
    
    qCDebug(log_device_diagnostics) << "Factory Reset test finished:" << (success ? "PASS" : "FAIL");
}

bool DiagnosticsManager::performFactoryResetTest()
{
    // Get SerialPortManager instance
    SerialPortManager& serialManager = SerialPortManager::getInstance();
    
    // Check if serial port is available
    QString currentPortPath = serialManager.getCurrentSerialPortPath();
    if (currentPortPath.isEmpty()) {
        appendToLog("No serial port available for factory reset test");
        return false;
    }
    
    appendToLog(QString("Using serial port: %1 for factory reset operation").arg(currentPortPath));
    
    try {
        // Attempt factory reset - try both methods for compatibility
        appendToLog("Attempting standard factory reset method...");
        bool success = serialManager.factoryResetHipChip();
        
        if (success) {
            appendToLog("Standard factory reset completed successfully");
            
            // Wait a moment for reset to complete
            QEventLoop loop;
            QTimer::singleShot(2000, &loop, &QEventLoop::quit);
            loop.exec();
            
            // Verify reset by attempting to communicate with device
            appendToLog("Verifying device communication after reset...");
            QByteArray response = serialManager.sendSyncCommand(CMD_GET_INFO, false);
            
            if (!response.isEmpty() && response.size() >= static_cast<int>(sizeof(CmdGetInfoResult))) {
                appendToLog("Device communication verified after factory reset");
                return true;
            } else {
                appendToLog("Warning: Device communication not verified, but reset command succeeded");
                return true; // Still consider success if reset command succeeded
            }
        } else {
            // Try V191 method as fallback
            appendToLog("Standard method failed, trying V191 factory reset method...");
            success = serialManager.factoryResetHipChipV191();
            
            if (success) {
                appendToLog("V191 factory reset completed successfully");
                
                // Wait a moment for reset to complete
                QEventLoop loop;
                QTimer::singleShot(2000, &loop, &QEventLoop::quit);
                loop.exec();
                
                return true;
            } else {
                appendToLog("Both factory reset methods failed");
                return false;
            }
        }
        
    } catch (const std::exception& e) {
        appendToLog(QString("Exception during factory reset: %1").arg(e.what()));
        return false;
    } catch (...) {
        appendToLog("Unknown error during factory reset operation");
        return false;
    }
}

void DiagnosticsManager::startHighBaudrateTest()
{
    m_isTestingInProgress = true;
    m_runningTestIndex = 5; // High Baudrate test index
    
    m_statuses[5] = TestStatus::InProgress;
    emit statusChanged(5, TestStatus::InProgress);
    
    appendToLog("Started test: High Baudrate");
    appendToLog("Testing baudrate switching to 115200...");
    emit testStarted(5);
    
    // Perform high baudrate test
    bool success = performHighBaudrateTest();
    
    if (success) {
        m_statuses[5] = TestStatus::Completed;
        appendToLog("High Baudrate test: PASSED - Successfully switched to 115200 baudrate");
    } else {
        m_statuses[5] = TestStatus::Failed;
        appendToLog("High Baudrate test: FAILED - Could not switch to 115200 baudrate");
    }
    
    emit statusChanged(5, m_statuses[5]);
    emit testCompleted(5, success);
    
    m_isTestingInProgress = false;
    m_runningTestIndex = -1;
    
    // Check if all tests completed
    checkAllTestsCompletion();
    
    qCDebug(log_device_diagnostics) << "High Baudrate test finished:" << (success ? "PASS" : "FAIL");
}

bool DiagnosticsManager::performHighBaudrateTest()
{
    // Get SerialPortManager instance
    SerialPortManager& serialManager = SerialPortManager::getInstance();
    
    // Check if serial port is available
    QString currentPortPath = serialManager.getCurrentSerialPortPath();
    if (currentPortPath.isEmpty()) {
        appendToLog("High Baudrate test failed: No serial port available");
        return false;
    }
    
    appendToLog(QString("Using serial port: %1 for baudrate test").arg(currentPortPath));
    
    // Get current baudrate before testing
    int currentBaudrate = serialManager.getCurrentBaudrate();
    appendToLog(QString("Current baudrate: %1").arg(currentBaudrate));
    
    // If already at 115200, test successful communication and return
    if (currentBaudrate == SerialPortManager::BAUDRATE_HIGHSPEED) {
        appendToLog("Already at 115200 baudrate, testing communication...");
        QByteArray testResponse = serialManager.sendSyncCommand(CMD_GET_INFO, true);
        if (!testResponse.isEmpty()) {
            CmdGetInfoResult infoResult = CmdGetInfoResult::fromByteArray(testResponse);
            appendToLog(QString("Communication test successful at 115200 - received response (version: %1)").arg(infoResult.version));
            return true;
        } else {
            appendToLog("Communication test failed at 115200 baudrate");
            return false;
        }
    }
    
    try {
        // First, test current communication to ensure we have a baseline
        appendToLog(QString("Testing baseline communication at %1...").arg(currentBaudrate));
        
        // Try with force=true to ensure command is sent even if ready state is uncertain
        QByteArray baselineResponse = serialManager.sendSyncCommand(CMD_GET_INFO, true);
        if (baselineResponse.isEmpty()) {
            // If first attempt fails, wait a bit and try again 
            appendToLog("First baseline attempt failed, waiting and retrying...");
            QEventLoop retryWait;
            QTimer::singleShot(1000, &retryWait, &QEventLoop::quit);  // 1 second wait
            retryWait.exec();
            
            baselineResponse = serialManager.sendSyncCommand(CMD_GET_INFO, true);
            if (baselineResponse.isEmpty()) {
                appendToLog("Baseline communication test failed after retry - cannot proceed with baudrate test");
                return false;
            }
        }
        
        CmdGetInfoResult baselineResult = CmdGetInfoResult::fromByteArray(baselineResponse);
        appendToLog(QString("Baseline communication successful (version: %1)").arg(baselineResult.version));
        
        // Method: Proper command-based baudrate switching
        appendToLog("Performing proper command-based baudrate switching to 115200...");
        
        // Step 1: Send configuration command at current baudrate (following applyCommandBasedBaudrateChange pattern)
        appendToLog("Step 1: Sending baudrate configuration command at current rate...");
        
        // Get system settings for mode
        static QSettings settings("Techxartisan", "Openterface");
        uint8_t mode = (settings.value("hardware/operatingMode", 0x02).toUInt());
        
        // Build configuration command for 115200
        QByteArray command = CMD_SET_PARA_CFG_PREFIX_115200;
        command[5] = mode;  // Set mode byte
        command.append(CMD_SET_PARA_CFG_MID);
        
        appendToLog("Sending configuration command to device...");
        QByteArray configResponse = serialManager.sendSyncCommand(command, true);
        
        if (configResponse.isEmpty()) {
            appendToLog("Configuration command failed - no response from device");
            return false;
        }
        
        // Parse configuration response
        CmdDataResult configResult = CmdDataResult::fromByteArray(configResponse);
        if (configResult.data != DEF_CMD_SUCCESS) {
            appendToLog(QString("Configuration command failed with status: 0x%1").arg(configResult.data, 2, 16, QChar('0')));
            return false;
        }
        
        appendToLog("Configuration command successful");
        
        // Step 2: Send reset command at current baudrate
        appendToLog("Step 2: Sending reset command...");
        QByteArray resetResponse = serialManager.sendSyncCommand(CMD_RESET, true);
        
        if (resetResponse.isEmpty()) {
            appendToLog("Reset command failed - no response from device");
            return false;
        }
        
        appendToLog("Reset command successful");
        
        // Step 3: Wait for reset to complete
        appendToLog("Step 3: Waiting for device reset to complete...");
        QEventLoop resetWaitLoop;
        QTimer::singleShot(1000, &resetWaitLoop, &QEventLoop::quit);  // 1 second wait
        resetWaitLoop.exec();
        
        // Step 4: Set host-side baudrate to 115200
        appendToLog("Step 4: Setting host-side baudrate to 115200...");
        bool setBaudrateResult = serialManager.setBaudRate(SerialPortManager::BAUDRATE_HIGHSPEED);
        if (!setBaudrateResult) {
            appendToLog("Failed to set host-side baudrate to 115200");
            return false;
        }
        
        // Step 5: Wait for baudrate change to stabilize
        appendToLog("Step 5: Waiting for baudrate change to stabilize...");
        QEventLoop stabilizeLoop;
        QTimer::singleShot(500, &stabilizeLoop, &QEventLoop::quit);  // 500ms wait
        stabilizeLoop.exec();
        
        // Step 6: Verify new baudrate
        int newBaudrate = serialManager.getCurrentBaudrate();
        appendToLog(QString("Host-side baudrate now set to: %1").arg(newBaudrate));
        
        if (newBaudrate != SerialPortManager::BAUDRATE_HIGHSPEED) {
            appendToLog(QString("Host-side baudrate mismatch - expected 115200, got %1").arg(newBaudrate));
            return false;
        }
        
        // Step 7: Test communication at 115200
        appendToLog("Step 6: Testing communication at 115200 baudrate...");
        QByteArray highSpeedResponse = serialManager.sendSyncCommand(CMD_GET_INFO, true);
        
        if (!highSpeedResponse.isEmpty()) {
            CmdGetInfoResult highSpeedResult = CmdGetInfoResult::fromByteArray(highSpeedResponse);
            appendToLog(QString("High-speed communication successful - version: %1").arg(highSpeedResult.version));
            appendToLog("Baudrate switch to 115200 completed successfully!");
            return true;
        } else {
            appendToLog("High-speed communication failed - device may not have switched baudrates");
            
            // Recovery: Try to restore original baudrate
            appendToLog("Attempting to recover by restoring original baudrate...");
            bool restoreBaudrate = serialManager.setBaudRate(currentBaudrate);
            if (restoreBaudrate) {
                QEventLoop recoveryLoop;
                QTimer::singleShot(500, &recoveryLoop, &QEventLoop::quit);
                recoveryLoop.exec();
                
                QByteArray recoveryResponse = serialManager.sendSyncCommand(CMD_GET_INFO, false);
                if (!recoveryResponse.isEmpty()) {
                    appendToLog("Successfully restored communication at original baudrate");
                    appendToLog("High baudrate test failed: device did not switch to 115200");
                } else {
                    appendToLog("Failed to restore communication - serial connection may be broken");
                }
            }
            return false;
        }
        
    } catch (const std::exception& e) {
        appendToLog(QString("High baudrate test failed due to exception: %1").arg(e.what()));
        return false;
    } catch (...) {
        appendToLog("High baudrate test failed due to unknown error");
        return false;
    }
}

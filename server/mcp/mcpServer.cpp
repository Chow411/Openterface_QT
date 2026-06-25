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

#include "mcpServer.h"
#include "mcpProtocol.h"
#include "mcpToolHandler.h"
#include "host/cameramanager.h"
#include "scripts/scriptRunner.h"
#include "scripts/scriptExecutor.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QSocketNotifier>
#include <unistd.h>

Q_LOGGING_CATEGORY(log_server_mcp, "opf.server.mcp")

McpServer::McpServer(QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_client(nullptr)
    , m_toolHandler(nullptr)
    , m_ownsToolHandler(false)
{
}

McpServer::~McpServer()
{
    stop();
}

bool McpServer::start(const QString &pipeName)
{
    if (m_server && m_server->isListening()) {
        qCWarning(log_server_mcp) << "MCP Server is already running";
        return true;
    }

    // Clean up any previous server
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }

    // Create tool handler if not injected
    if (!m_toolHandler) {
        m_toolHandler = new McpToolHandler(this);
        m_ownsToolHandler = true;
        applyPendingDependencies();
    }

    // Remove any stale socket file (Linux)
    QLocalServer::removeServer(pipeName);

    m_server = new QLocalServer(this);
    if (!m_server->listen(pipeName)) {
        qCCritical(log_server_mcp) << "Failed to start MCP Server on pipe:" << pipeName
                                    << "Error:" << m_server->errorString();
        m_server->deleteLater();
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QLocalServer::newConnection,
            this, &McpServer::onNewConnection);

    qCInfo(log_server_mcp) << "MCP Server started on pipe:" << pipeName;
    emit started();
    emit logMessage("MCP Server started on: " + pipeName);
    return true;
}

void McpServer::stop()
{
    // Stop stdio mode if active
    if (m_stdioMode) {
        if (m_stdinPollTimer) {
            m_stdinPollTimer->stop();
            delete m_stdinPollTimer;
            m_stdinPollTimer = nullptr;
        }
        if (m_stdinFile) {
            m_stdinFile->close();
            delete m_stdinFile;
            m_stdinFile = nullptr;
        }
        if (m_stdoutFile) {
            m_stdoutFile->flush();
            m_stdoutFile->close();
            delete m_stdoutFile;
            m_stdoutFile = nullptr;
        }
        m_stdioMode = false;
        qCInfo(log_server_mcp) << "MCP stdio transport stopped";
        emit stopped();
        emit logMessage("MCP stdio transport stopped");
        return;
    }

    if (!m_server) return;

    // Disconnect client if connected
    if (m_client) {
        m_client->disconnectFromServer();
        m_client->deleteLater();
        m_client = nullptr;
    }

    m_server->close();
    m_server->deleteLater();
    m_server = nullptr;

    qCInfo(log_server_mcp) << "MCP Server stopped";
    emit stopped();
    emit logMessage("MCP Server stopped");
}

bool McpServer::isRunning() const
{
    if (m_stdioMode) return m_stdinPollTimer && m_stdinPollTimer->isActive();
    return m_server && m_server->isListening();
}

void McpServer::setCameraManager(CameraManager* cameraManager)
{
    m_pendingCameraManager = cameraManager;
    if (m_toolHandler) {
        m_toolHandler->setCameraManager(cameraManager);
    }
}

void McpServer::setScriptRunner(ScriptRunner* scriptRunner)
{
    m_pendingScriptRunner = scriptRunner;
    if (m_toolHandler) {
        m_toolHandler->setScriptRunner(scriptRunner);
    }
}

void McpServer::setScriptExecutor(ScriptExecutor* scriptExecutor)
{
    m_pendingScriptExecutor = scriptExecutor;
    if (m_toolHandler) {
        m_toolHandler->setScriptExecutor(scriptExecutor);
    }
}

void McpServer::setToolHandler(McpToolHandler* handler)
{
    if (m_ownsToolHandler && m_toolHandler) {
        m_toolHandler->deleteLater();
    }
    m_toolHandler = handler;
    m_ownsToolHandler = false;
    applyPendingDependencies();
}

void McpServer::applyPendingDependencies()
{
    if (!m_toolHandler) return;
    if (m_pendingCameraManager) m_toolHandler->setCameraManager(m_pendingCameraManager);
    if (m_pendingScriptRunner)  m_toolHandler->setScriptRunner(m_pendingScriptRunner);
    if (m_pendingScriptExecutor) m_toolHandler->setScriptExecutor(m_pendingScriptExecutor);
}

McpToolHandler* McpServer::toolHandler() const
{
    return m_toolHandler;
}

// ---------------------------------------------------------------------------
// Connection handling
// ---------------------------------------------------------------------------

void McpServer::onNewConnection()
{
    // MCP supports only one client at a time
    if (m_client) {
        QLocalSocket* extra = m_server->nextPendingConnection();
        if (extra) {
            qCWarning(log_server_mcp) << "Rejecting additional client — MCP only supports one connection";
            extra->disconnectFromServer();
            extra->deleteLater();
        }
        return;
    }

    m_client = m_server->nextPendingConnection();
    connect(m_client, &QLocalSocket::readyRead,
            this, &McpServer::onReadyRead);
    connect(m_client, &QLocalSocket::disconnected,
            this, &McpServer::onClientDisconnected);

    qCInfo(log_server_mcp) << "MCP client connected";
    emit logMessage("MCP client connected");
}

void McpServer::onReadyRead()
{
    if (!m_client) return;

    while (m_client->canReadLine()) {
        QString line = QString::fromUtf8(m_client->readLine()).trimmed();
        if (line.isEmpty()) continue;

        qCDebug(log_server_mcp) << "Received:" << line.left(200);
        handleMessage(line, m_client);
    }
}
void McpServer::onClientDisconnected()
{
    qCInfo(log_server_mcp) << "MCP client disconnected";
    emit logMessage("MCP client disconnected");

    if (m_client) {
        m_client->deleteLater();
        m_client = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Message dispatching
// ---------------------------------------------------------------------------

void McpServer::handleMessage(const QString& jsonLine, QIODevice* device)
{
    McpProtocol::Request req;
    if (!McpProtocol::parseRequest(jsonLine.toUtf8(), req)) {
        // Invalid JSON — send a parse error (no id available)
        QJsonObject errResp = McpProtocol::buildError(
            QVariant(), JSONRPC_ERROR_PARSE_ERROR,
            "Parse error: invalid JSON");
        sendResponse(errResp, device);
        return;
    }

    // Notifications have no id — no response needed
    if (req.isNotification) {
        qCDebug(log_server_mcp) << "Notification received:" << req.method;
        if (req.method == MCP_METHOD_INITIALIZED) {
            qCInfo(log_server_mcp) << "Client initialized notification received";
            emit logMessage("MCP client initialization complete");
        }
        return;
    }

    // Dispatch based on method
    QJsonObject response;

    if (req.method == MCP_METHOD_INITIALIZE) {
        QJsonObject result = McpProtocol::buildInitializeResult();
        response = McpProtocol::buildResult(req.id, result);
        qCInfo(log_server_mcp) << "Client initialized successfully";
        emit logMessage("MCP client initialized");

    } else if (req.method == MCP_METHOD_TOOLS_LIST) {
        if (m_toolHandler) {
            QJsonArray tools = m_toolHandler->listTools();
            QJsonObject result;
            result["tools"] = tools;
            response = McpProtocol::buildResult(req.id, result);
        } else {
            response = McpProtocol::buildError(
                req.id, JSONRPC_ERROR_INTERNAL_ERROR,
                "Tool handler not initialized");
        }

    } else if (req.method == MCP_METHOD_TOOLS_CALL) {
        if (!m_toolHandler) {
            response = McpProtocol::buildError(
                req.id, JSONRPC_ERROR_INTERNAL_ERROR,
                "Tool handler not initialized");
        } else {
            QString toolName = req.params.value("name").toString();
            QJsonObject toolArgs = req.params.value("arguments").toObject();

            if (toolName.isEmpty()) {
                response = McpProtocol::buildError(
                    req.id, JSONRPC_ERROR_INVALID_PARAMS,
                    "Missing tool name in 'name' field");
            } else {
                QJsonObject toolResult = m_toolHandler->callTool(toolName, toolArgs);
                response = McpProtocol::buildResult(req.id, toolResult);
            }
        }

    } else if (req.method == MCP_METHOD_PING) {
        QJsonObject result;
        result["status"] = "ok";
        response = McpProtocol::buildResult(req.id, result);

    } else {
        response = McpProtocol::buildError(
            req.id, JSONRPC_ERROR_METHOD_NOT_FOUND,
            "Method not found: " + req.method);
    }

    sendResponse(response, device);
}

void McpServer::sendResponse(const QJsonObject& response, QIODevice* device)
{
    QByteArray data = McpProtocol::serialize(response);

    if (device == m_stdoutFile) {
        // For stdio mode, use POSIX write() to stdout to avoid QFile pipe issues
        ssize_t written = ::write(STDOUT_FILENO, data.constData(), data.size());
        if (written == -1) {
            qCWarning(log_server_mcp) << "Failed to write to stdout";
        }
        // Also write to QFile for consistency (sockets use QFile path)
        return;
    }

    if (!device || !device->isOpen()) {
        qCWarning(log_server_mcp) << "Cannot send response — device not open";
        return;
    }

    device->write(data);
    if (QLocalSocket* s = qobject_cast<QLocalSocket*>(device)) {
        s->flush();
    }

    qCDebug(log_server_mcp) << "Sent response:" << data.left(200);
}

// ---------------------------------------------------------------------------
// Stdio transport
// ---------------------------------------------------------------------------

bool McpServer::startStdio()
{
    if (m_stdioMode) {
        qCWarning(log_server_mcp) << "MCP stdio mode already active";
        return true;
    }

    // Create tool handler if not injected
    if (!m_toolHandler) {
        m_toolHandler = new McpToolHandler(this);
        m_ownsToolHandler = true;
        applyPendingDependencies();
    }

    // Wrap stdin/stdout as QFile objects
    m_stdinFile  = new QFile(this);
    if (!m_stdinFile->open(stdin, QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        qCCritical(log_server_mcp) << "Failed to open stdin for MCP stdio transport";
        delete m_stdinFile;
        m_stdinFile = nullptr;
        return false;
    }

    m_stdoutFile = new QFile(this);
    if (!m_stdoutFile->open(stdout, QIODevice::WriteOnly | QIODevice::Unbuffered)) {
        qCCritical(log_server_mcp) << "Failed to open stdout for MCP stdio transport";
        m_stdinFile->close();
        delete m_stdinFile;
        m_stdinFile = nullptr;
        delete m_stdoutFile;
        m_stdoutFile = nullptr;
        return false;
    }

    // Watch stdin using a polling timer (more reliable than QSocketNotifier for stdin)
    m_stdinPollTimer = new QTimer(this);
    connect(m_stdinPollTimer, &QTimer::timeout,
            this, &McpServer::onStdinReadyRead);
    m_stdinPollTimer->start(10);  // Poll every 10ms

    m_stdioMode = true;
    qCInfo(log_server_mcp) << "MCP stdio transport started";
    emit started();
    emit stdioReady();
    emit logMessage("MCP stdio transport started");
    return true;
}

void McpServer::onStdinReadyRead()
{
    if (!m_stdinFile || !m_stdoutFile) {
        return;
    }

    // Use POSIX read() directly — QFile::canReadLine() doesn't work on pipes
    // with Unbuffered mode (it checks an internal buffer that's always empty).
    char buffer[4096];
    ssize_t bytesRead = ::read(STDIN_FILENO, buffer, sizeof(buffer));

    if (bytesRead > 0) {
        m_stdinBuffer.append(buffer, bytesRead);
    } else if (bytesRead == 0) {
        // EOF — stdin closed
        qCInfo(log_server_mcp) << "stdio EOF received, stopping";
        m_stdinPollTimer->stop();
        return;
    } else {
        return;  // EAGAIN or error, try again on next timer tick
    }

    // Process complete lines from the buffer
    while (true) {
        int newlinePos = m_stdinBuffer.indexOf('\n');
        if (newlinePos == -1) {
            break;  // No complete line yet
        }

        QByteArray line = m_stdinBuffer.left(newlinePos).trimmed();
        m_stdinBuffer.remove(0, newlinePos + 1);

        if (!line.isEmpty()) {
            qCInfo(log_server_mcp) << "stdio received:" << line.left(200);
            handleMessage(QString::fromUtf8(line), m_stdoutFile);
        }
    }
}


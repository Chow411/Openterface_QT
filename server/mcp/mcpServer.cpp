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

void McpServer::handleMessage(const QString& jsonLine, QLocalSocket* client)
{
    McpProtocol::Request req;
    if (!McpProtocol::parseRequest(jsonLine.toUtf8(), req)) {
        // Invalid JSON — send a parse error (no id available)
        QJsonObject errResp = McpProtocol::buildError(
            QVariant(), JSONRPC_ERROR_PARSE_ERROR,
            "Parse error: invalid JSON");
        sendResponse(errResp, client);
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

    sendResponse(response, client);
}

void McpServer::sendResponse(const QJsonObject& response, QLocalSocket* client)
{
    if (!client || client->state() != QLocalSocket::ConnectedState) {
        qCWarning(log_server_mcp) << "Cannot send response — client not connected";
        return;
    }

    QByteArray data = McpProtocol::serialize(response);
    client->write(data);
    client->flush();

    qCDebug(log_server_mcp) << "Sent response:" << data.left(200);
}

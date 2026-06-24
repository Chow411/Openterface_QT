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

#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <memory>

class McpProtocol;
class McpToolHandler;
class CameraManager;
class ScriptRunner;
class ScriptExecutor;
class ASTNode;

class McpServer : public QObject {
    Q_OBJECT

public:
    explicit McpServer(QObject *parent = nullptr);
    ~McpServer() override;

    // --- Lifecycle ---

    /**
     * Start listening on the named pipe.
     * @param pipeName  Pipe name (e.g. "openterface-mcp").
     *                  Linux:  /tmp/openterface-mcp (Unix domain socket)
     *                  Windows: \\.\pipe\openterface-mcp
     * @return true if started successfully, false on failure.
     */
    bool start(const QString &pipeName = "openterface-mcp");

    /** Stop listening and disconnect any client. */
    void stop();

    /** Whether the server is currently listening. */
    bool isRunning() const;

    // --- Dependency Injection ---

    /** Set the CameraManager (used for screen capture tools). */
    void setCameraManager(CameraManager* cameraManager);

    /** Set the ScriptRunner (used for script execution tools). */
    void setScriptRunner(ScriptRunner* scriptRunner);

    /** Set the ScriptExecutor (used for script execution tools). */
    void setScriptExecutor(ScriptExecutor* scriptExecutor);

    /** Inject a pre-built tool handler (optional — creates one internally if null). */
    void setToolHandler(McpToolHandler* handler);

    /** Provide access to the internal tool handler for additional configuration. */
    McpToolHandler* toolHandler() const;

signals:
    /** Emitted when the server starts listening successfully. */
    void started();

    /** Emitted when the server is stopped. */
    void stopped();

    /** Emitted for informational/log messages. */
    void logMessage(const QString& message);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    QLocalServer* m_server = nullptr;
    QLocalSocket* m_client = nullptr;   // MCP supports a single client
    McpToolHandler* m_toolHandler = nullptr;
    bool m_ownsToolHandler = false;     // True if we created m_toolHandler ourselves

    // Pending dependencies — stored until the tool handler is created
    CameraManager*  m_pendingCameraManager = nullptr;
    ScriptRunner*   m_pendingScriptRunner = nullptr;
    ScriptExecutor* m_pendingScriptExecutor = nullptr;

    void applyPendingDependencies();

    /**
     * Handle a single JSON-RPC request and send the response to the client.
     * Dispatches: initialize, notifications/initialized, tools/list, tools/call, ping
     */
    void handleMessage(const QString& jsonLine, QLocalSocket* client);

    /** Serialize and send a JSON response, appending a newline. */
    void sendResponse(const QJsonObject& response, QLocalSocket* client);
};

#endif // MCP_SERVER_H

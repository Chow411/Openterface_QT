# MCP Server HTTP Transport Implementation Plan

## Overview

This document outlines the implementation plan for adding HTTP-based transport to the existing MCP server, supporting both Streamable HTTP (2025-03-26) and SSE (2024-11-05) specifications while maintaining compatibility with the existing Named Pipe transport.

## Architecture

The implementation follows a modular architecture where:
- `McpHttpTransport` handles all HTTP communication
- `McpServer` is extended to manage both Named Pipe and HTTP transports
- Existing `McpToolHandler` remains unchanged and is shared between transports

```
┌─────────────────────────────────────────────────────────────┐
│                    McpServer (Extended)                       │
│                                                              │
│  ┌─────────────────┐    ┌────────────────────────────────┐ │
│  │  QLocalServer   │    │        QHttpServer             │ │
│  │  (Named Pipe)   │    │   (HTTP - Port 8080)           │ │
│  │                 │    │                                │ │
│  │  /tmp/          │    │  POST /mcp → Streamable HTTP  │ │
│  │  openterface-mcp│    │  GET /sse  → SSE stream       │ │
│  │                 │    │  POST /messages → SSE msg     │ │
│  └────────┬────────┘    └──────────────┬─────────────────┘ │
│           │                            │                    │
│           └────────────┬───────────────┘                    │
│                        │                                    │
│              ┌─────────▼──────────┐                         │
│              │   McpToolHandler   │                         │
│              │   (Shared)         │                         │
│              └────────────────────┘                         │
└─────────────────────────────────────────────────────────────┘
```

## File Changes

### New Files

| File | Purpose |
|------|---------|
| `server/mcp/mcpHttpTransport.h` | HTTP transport class definition |
| `server/mcp/mcpHttpTransport.cpp` | HTTP routing, session management, and SSE streaming |

### Modified Files

| File | Changes |
|------|---------|
| `CMakeLists.txt` | Add Qt6::HttpServer dependency |
| `cmake/SourceFiles.cmake` | Add new HTTP transport files |
| `server/mcp/mcpServer.h` | Add HTTP transport control methods |
| `server/mcp/mcpServer.cpp` | Implement HTTP transport integration |
| `server/mcp/mcpConstants.h` | Add HTTP-related constants |

## Implementation Details

### 1. McpHttpTransport Class

```cpp
class McpHttpTransport : public QObject {
    Q_OBJECT

public:
    explicit McpHttpTransport(McpToolHandler* handler, QObject* parent = nullptr);
    
    bool start(quint16 port = 8080);
    void stop();
    bool isRunning() const;

private:
    // HTTP routing handlers
    QHttpServerResponse handleMcp(const QHttpServerRequest& request);  // Streamable HTTP
    QHttpServerResponse handleSse(const QHttpServerRequest& request);  // GET /sse
    QHttpServerResponse handleMessages(const QHttpServerRequest& request);  // POST /messages
    
    // Session management
    struct Session {
        QString id;
        bool initialized = false;
        QHttpServerResponse* sseStream = nullptr;
    };
    
    QMap<QString, Session*> m_sessions;
    McpToolHandler* m_toolHandler;
    QHttpServer* m_httpServer = nullptr;
    
    // Helper methods
    QString generateSessionId() const;
    QJsonObject processRequest(const QJsonObject& request, Session* session);
    void sendSseEvent(Session* session, const QJsonObject& message);
    QByteArray formatSseEvent(const QString& event, const QByteArray& data);
};
```

### 2. McpServer Extensions

Add the following methods to `mcpServer.h`:

```cpp
// HTTP transport control
bool startHttp(quint16 port = 8080);
void stopHttp();
bool isHttpRunning() const;

// Unified control
bool startAll(const QString& pipeName = "openterface-mcp", quint16 httpPort = 8080);
void stopAll();

private:
McpHttpTransport* m_httpTransport = nullptr;
```

### 3. HTTP Routing Specification

#### Streamable HTTP (2025-03-26)

**Endpoint**: `POST /mcp`

Request headers:
- `Content-Type: application/json`
- `Accept: application/json` or `text/event-stream`

Response headers:
- `Content-Type: application/json` or `text/event-stream`
- `Mcp-Session-Id: <session-id>`

#### SSE Transport (2024-11-05)

**Establish SSE stream**: `GET /sse`

Response:
```
event: endpoint
data: /messages?sessionId=<session-id>
```

**Send messages**: `POST /messages?sessionId=<session-id>`

### 4. Session Management

- Each HTTP client gets a unique UUID session ID
- Streamable HTTP: session ID passed in `Mcp-Session-Id` header
- SSE: session ID passed in URL query parameter
- Support multiple concurrent HTTP clients

## Implementation Steps

### Phase 1: Infrastructure Setup (Day 1)

1. Install Qt6 HttpServer development package
2. Create `mcpHttpTransport.h/cpp` skeleton files
3. Update CMake configuration
4. Add HTTP transport methods to `McpServer`

### Phase 2: Streamable HTTP Implementation (Day 2)

1. Implement `POST /mcp` routing
2. Implement JSON response mode
3. Implement session management
4. Basic curl testing

### Phase 3: SSE Implementation (Day 3)

1. Implement `GET /sse` for SSE stream establishment
2. Implement `POST /messages` for SSE message handling
3. Implement SSE event formatting
4. Python client testing

### Phase 4: Integration and Testing (Day 4)

1. Command-line argument integration
2. Multi-client concurrency testing
3. Named Pipe + HTTP coexistence testing
4. Claude Code configuration testing

## Testing Strategy

### Unit Tests

```bash
# Test Streamable HTTP
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'

# Test SSE stream
curl -N http://localhost:8080/sse
```

### Integration Tests

- Verify Named Pipe client connectivity
- Verify HTTP client connectivity
- Test concurrent connections
- Validate tool call responses

## Risks and Considerations

1. **Qt6 HttpServer availability**: Requires Qt 6.4+, current version 6.9.3 satisfies this
2. **Thread safety**: Ensure `McpToolHandler` supports concurrent access
3. **SSE connection management**: Proper handling of connection drops and cleanup
4. **CORS considerations**: For local development, CORS can be ignored

## Future Extensions

- WebSocket transport support
- Authentication and authorization mechanisms
- Rate limiting
- Connection pooling
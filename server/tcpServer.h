#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QFile>

enum ActionCommand {
    CmdUnknow = -1,
    FullScreenCapture,
    AreaScreenCapture,
    Click,
    Send,
    SetCapsLockState,
    SetNumLockState,
    SetScrollLockState,
    CmdGetLastImage
};

class TcpServer : public QTcpServer {
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    void startServer(quint16 port);

public slots:
    void handleImgPath(const QString& imagePath);

private slots:
    void onNewConnection();
    void onReadyRead();
    
private:
    QTcpSocket *currentClient;
    void captureFullScreen();
    QString lastImgPath;
    ActionCommand parseCommand(const QByteArray& data);
    void sendImageToClient();
    void processCommand(ActionCommand cmd);
};


#endif // TCPSERVER_H

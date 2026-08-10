#ifndef NETCLIENT_H
#define NETCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>

// 联机测试用网络客户端：QTcpSocket + JSON 行协议（`\n` 分隔）。
// 服务器：server/（Node.js），TCP 端口默认 8764。
// 说明：正式方案为 Qt WebSockets（Qt6::WebSockets 模块，本机暂未安装），
// 当前用 QTcpSocket 做基本通信测试；接口设计（connectToServer/sendJson/
// messageReceived）与未来 WebSocket 版保持一致。
class NetClient : public QObject
{
    Q_OBJECT
public:
    explicit NetClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool sendJson(const QJsonObject &obj);
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void messageReceived(const QJsonObject &msg);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);

private:
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buf;   // 按 '\n' 切分累积的缓冲
};

#endif // NETCLIENT_H

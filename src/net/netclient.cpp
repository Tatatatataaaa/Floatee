#include "netclient.h"
#include <QJsonDocument>

NetClient::NetClient(QObject *parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &NetClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetClient::onSocketError);
}

void NetClient::connectToServer(const QString &host, quint16 port)
{
    m_socket->connectToHost(host, port);
}

void NetClient::disconnectFromServer()
{
    m_socket->disconnectFromHost();
}

bool NetClient::sendJson(const QJsonObject &obj)
{
    if (!isConnected())
        return false;
    const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    m_socket->write(line);
    return true;
}

bool NetClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetClient::onConnected()
{
    emit connected();
}

void NetClient::onDisconnected()
{
    m_buf.clear();
    emit disconnected();
}

void NetClient::onReadyRead()
{
    m_buf.append(m_socket->readAll());
    int idx;
    while ((idx = m_buf.indexOf('\n')) >= 0) {
        const QByteArray line = m_buf.left(idx).trimmed();
        m_buf.remove(0, idx + 1);
        if (line.isEmpty())
            continue;
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;   // 忽略无法解析的行（日志可选）
        emit messageReceived(doc.object());
    }
    if (m_buf.size() > 65536)
        m_buf.clear();
}

void NetClient::onSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err)
    emit errorOccurred(m_socket->errorString());
}

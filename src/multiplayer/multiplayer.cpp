#include "multiplayer.h"

#include <QJsonDocument>
#include <QJsonArray>

Multiplayer::Multiplayer(QObject *parent)
    : QObject(parent)
{
    m_client = new NetClient(this);
    connect(m_client, &NetClient::connected, this, &Multiplayer::onConnected);
    connect(m_client, &NetClient::disconnected, this, &Multiplayer::onDisconnected);
    connect(m_client, &NetClient::messageReceived, this, &Multiplayer::onMessage);
    connect(m_client, &NetClient::errorOccurred, this, &Multiplayer::onNetError);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &Multiplayer::onReconnectTick);

    // 心跳：每 10s 发 ping，避免被服务器 30s 无消息超时踢出
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(10000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, [this]() {
        m_client->sendJson(QJsonObject{{QStringLiteral("type"), QStringLiteral("ping")}});
    });
}

void Multiplayer::init(const QString &clientId, const QString &deviceId, const QString &savedServer)
{
    m_clientId = clientId;
    m_deviceId = deviceId;
    if (!savedServer.isEmpty()) {
        const int colon = savedServer.lastIndexOf(QLatin1Char(':'));
        if (colon > 0) {
            m_host = savedServer.left(colon);
            const quint16 p = savedServer.mid(colon + 1).toUShort();
            m_port = p != 0 ? p : 8764;
        } else {
            m_host = savedServer;
        }
    }
}

QString Multiplayer::serverAddress() const
{
    return QStringLiteral("%1:%2").arg(m_host).arg(m_port);
}

void Multiplayer::connectTo(const QString &host, quint16 port)
{
    stopReconnect();
    m_host = host;
    m_port = port;
    m_shouldReconnect = true;
    m_client->connectToServer(host, port);
}

void Multiplayer::disconnect()
{
    m_shouldReconnect = false;
    stopReconnect();
    m_heartbeatTimer->stop();
    m_client->disconnectFromServer();
    clearRoom();
    updateStatus();
}

void Multiplayer::send(const QJsonObject &obj)
{
    if (!m_client->sendJson(obj))
        emit notify(QStringLiteral("Multiplayer"), QStringLiteral("未连接服务器"), true);
}

// ── 房间操作 ─────────────────────────────────────────────────────────

void Multiplayer::createRoom(const QString &roomName)
{
    QJsonObject o{{QStringLiteral("type"), QStringLiteral("create_room")}};
    if (!roomName.isEmpty())
        o.insert(QStringLiteral("roomName"), roomName);
    send(o);
}

void Multiplayer::joinRoom(const QString &roomId, const QString &joinCode)
{
    QJsonObject o{{QStringLiteral("type"), QStringLiteral("join_room")},
                  {QStringLiteral("roomId"), roomId}};
    if (!joinCode.isEmpty())
        o.insert(QStringLiteral("joinCode"), joinCode);
    send(o);
}

void Multiplayer::leaveRoom()
{
    send(QJsonObject{{QStringLiteral("type"), QStringLiteral("leave_room")}});
    clearRoom();
    updateStatus();
}

void Multiplayer::listRooms()
{
    send(QJsonObject{{QStringLiteral("type"), QStringLiteral("list_rooms")}});
}

// ── 网络事件 ─────────────────────────────────────────────────────────

void Multiplayer::onConnected()
{
    m_connected = true;
    m_reconnectAttempts = 0;
    m_heartbeatTimer->start();
    updateStatus();
    // 握手
    m_client->sendJson(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("hello")},
        {QStringLiteral("clientId"), m_clientId},
        {QStringLiteral("deviceId"), m_deviceId},
        {QStringLiteral("displayName"), m_clientId},
    });
}

void Multiplayer::onDisconnected()
{
    m_connected = false;
    m_heartbeatTimer->stop();
    clearRoom();
    if (m_shouldReconnect)
        startReconnect();
    updateStatus();
}

void Multiplayer::onNetError(const QString &e)
{
    if (!m_connected)
        emit notify(QStringLiteral("Multiplayer"), QStringLiteral("连接失败: %1").arg(e), true);
}

void Multiplayer::startReconnect()
{
    // 指数退避：1s/2s/4s/8s/16s/30s 上限
    const int secs = qMin(30, 1 << m_reconnectAttempts);
    m_reconnectAttempts++;
    m_reconnectTimer->start(secs * 1000);
}

void Multiplayer::stopReconnect()
{
    m_reconnectTimer->stop();
}

void Multiplayer::onReconnectTick()
{
    if (m_shouldReconnect && !m_connected)
        m_client->connectToServer(m_host, m_port);
}

// ── 消息分发 ─────────────────────────────────────────────────────────

void Multiplayer::onMessage(const QJsonObject &msg)
{
    const QString type = msg.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("welcome")) {
        emit notify(QStringLiteral("Multiplayer"),
                    QStringLiteral("已连接服务器 %1").arg(serverAddress()), false);
        updateStatus();
        return;
    }
    if (type == QLatin1String("room_created")) {
        m_roomId = msg.value(QStringLiteral("roomId")).toString();
        m_joinCode = msg.value(QStringLiteral("joinCode")).toString();
        m_ownerToken = msg.value(QStringLiteral("ownerToken")).toString();
        emit notify(QStringLiteral("Multiplayer"),
                    QStringLiteral("房间已创建\n房间号: %1\n邀请码: %2\n（把邀请码分享给朋友即可加入）")
                        .arg(m_roomId, m_joinCode),
                    false);
        emit roomChanged();
        updateStatus();
        return;
    }
    if (type == QLatin1String("room_joined")) {
        m_roomId = msg.value(QStringLiteral("roomId")).toString();
        emit notify(QStringLiteral("Multiplayer"), QStringLiteral("已加入房间 %1").arg(m_roomId), false);
        emit roomChanged();
        updateStatus();
        return;
    }
    if (type == QLatin1String("room_left")) {
        clearRoom();
        emit roomChanged();
        updateStatus();
        return;
    }
    if (type == QLatin1String("room_closed")) {
        emit notify(QStringLiteral("Multiplayer"),
                    QStringLiteral("房间已关闭: %1").arg(msg.value(QStringLiteral("reason")).toString()), true);
        clearRoom();
        emit roomChanged();
        updateStatus();
        return;
    }
    if (type == QLatin1String("room_list")) {
        QList<QJsonObject> rooms;
        const QJsonArray arr = msg.value(QStringLiteral("rooms")).toArray();
        for (int i = 0; i < arr.size(); ++i)
            rooms.append(arr.at(i).toObject());
        emit roomListReceived(rooms);
        return;
    }
    if (type == QLatin1String("peer_joined") || type == QLatin1String("peer_left")) {
        emit roomChanged();   // M2 再维护完整成员表
        return;
    }
    if (type == QLatin1String("error")) {
        const QString code = msg.value(QStringLiteral("code")).toString();
        if (code == QLatin1String("device_busy")) {
            // 本设备已有联机进程：停止自动重连，避免反复提示/抢占
            m_shouldReconnect = false;
            stopReconnect();
            m_client->disconnectFromServer();
        }
        emit notify(QStringLiteral("Multiplayer"),
                    msg.value(QStringLiteral("message")).toString(), true);
        return;
    }
}

void Multiplayer::clearRoom()
{
    m_roomId.clear();
    m_joinCode.clear();
    m_ownerToken.clear();
    m_members.clear();
}

void Multiplayer::updateStatus()
{
    QString text;
    if (!m_connected)
        text = QStringLiteral("Status: 离线");
    else if (!inRoom())
        text = QStringLiteral("Status: 已连接 (%1)").arg(serverAddress());
    else
        text = QStringLiteral("Status: 房间 %1 (%2)").arg(m_roomId).arg(memberCount());
    emit statusChanged(text);
}

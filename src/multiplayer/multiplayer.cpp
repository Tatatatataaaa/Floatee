#include "multiplayer.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>

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

// ── M2：本地角色 / 皮肤 / 眼睛上报 ───────────────────────────────────

void Multiplayer::addLocalRole(const QString &skinName, int hue, double sat, double light)
{
    m_localRoleReady = false;   // 收到 role_added 前不上报 mouse/skin
    send(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("add_role")},
        {QStringLiteral("roleIndex"), 0},
        {QStringLiteral("roleName"), m_clientId},
        {QStringLiteral("skin"), skinName},
        {QStringLiteral("hue"), hue},
        {QStringLiteral("sat"), sat},
        {QStringLiteral("light"), light},
    });
}

void Multiplayer::updateLocalSkin(const QString &skinName)
{
    if (!m_localRoleReady)
        return;
    send(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("skin_update")},
        {QStringLiteral("roleId"), localRoleId()},
        {QStringLiteral("skin"), skinName},
    });
}

void Multiplayer::updateLocalMouse(float dx, float dy, int eye, float es)
{
    if (!m_localRoleReady)
        return;
    if (dx == m_lastDx && dy == m_lastDy && eye == m_lastEye && es == m_lastEs)
        return;
    // 节流：每 60ms 最多发一次（~16.7/s，低于服务器上限 20/s，避免撞限流）。
    // 眼睛分离渲染 + 条件重绘后，提高同步频率的边际成本很小（每次只渲染
    // 眼睛小区域，且仅真正变化时才整屏重绘），换来更平滑的远端眼睛跟随。
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastMouseSent < 60)
        return;
    m_lastMouseSent = now;
    m_lastDx = dx; m_lastDy = dy; m_lastEye = eye; m_lastEs = es;
    send(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("mouse")},
        {QStringLiteral("roleId"), localRoleId()},
        {QStringLiteral("dx"), double(dx)},
        {QStringLiteral("dy"), double(dy)},
        {QStringLiteral("eye"), eye},
        {QStringLiteral("es"), double(es)},
    });
}

void Multiplayer::sendEmoticon(int index)
{
    if (!m_localRoleReady)
        return;
    send(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("emoticon")},
        {QStringLiteral("roleId"), localRoleId()},
        {QStringLiteral("index"), index},
    });
}

void Multiplayer::sendChat(const QString &text)
{
    if (!m_localRoleReady || text.trimmed().isEmpty())
        return;
    send(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("chat")},
        {QStringLiteral("roleId"), localRoleId()},
        {QStringLiteral("text"), text.left(256)},
    });
}

void Multiplayer::kickMember(const QString &clientId)
{
    if (m_ownerToken.isEmpty() || m_roomId.isEmpty())
        return;                       // 非房主或不在房间
    send(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("kick_member")},
        {QStringLiteral("roomId"), m_roomId},
        {QStringLiteral("ownerToken"), m_ownerToken},
        {QStringLiteral("targetClientId"), clientId},
    });
}

void Multiplayer::upsertPeer(const QJsonObject &member)
{
    // member: { clientId, displayName, roles: [ {roleId, roleName, skin} ] }
    const QJsonArray roles = member.value(QStringLiteral("roles")).toArray();
    for (int i = 0; i < roles.size(); ++i) {
        const QJsonObject r = roles.at(i).toObject();
        const QString roleId = r.value(QStringLiteral("roleId")).toString();
        if (roleId.isEmpty())
            continue;
        PeerInfo p;
        p.roleId = roleId;
        p.roleName = r.value(QStringLiteral("roleName")).toString(roleId);
        p.skin = r.value(QStringLiteral("skin")).toString();
        p.hue = r.value(QStringLiteral("hue")).toInt(0);
        p.sat = r.value(QStringLiteral("sat")).toDouble(1.0);
        p.light = r.value(QStringLiteral("light")).toDouble(1.0);
        m_peers.insert(roleId, p);
    }
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
        // 现有成员角色进入 peer 表（不含自己，服务器 room_joined 的 members 含自己但可过滤）
        m_peers.clear();
        const QJsonArray members = msg.value(QStringLiteral("members")).toArray();
        for (int i = 0; i < members.size(); ++i) {
            const QJsonObject m = members.at(i).toObject();
            if (m.value(QStringLiteral("clientId")).toString() != m_clientId)
                upsertPeer(m);
        }
        emit notify(QStringLiteral("Multiplayer"), QStringLiteral("已加入房间 %1").arg(m_roomId), false);
        emit roomChanged();
        emit peersChanged();
        updateStatus();
        return;
    }
    if (type == QLatin1String("room_left")) {
        clearRoom();
        emit roomChanged();
        emit peersChanged();
        updateStatus();
        return;
    }
    if (type == QLatin1String("room_closed")) {
        emit notify(QStringLiteral("Multiplayer"),
                    QStringLiteral("房间已关闭: %1").arg(msg.value(QStringLiteral("reason")).toString()), true);
        clearRoom();
        emit roomChanged();
        emit peersChanged();
        updateStatus();
        return;
    }
    if (type == QLatin1String("role_added")) {
        m_localRoleReady = true;   // 本地角色已注册，可开始上报
        return;
    }
    if (type == QLatin1String("peer_joined")) {
        upsertPeer(msg.value(QStringLiteral("member")).toObject());
        emit peersChanged();
        return;
    }
    if (type == QLatin1String("peer_left")) {
        m_peers.remove(msg.value(QStringLiteral("roleId")).toString());
        emit peersChanged();
        return;
    }
    if (type == QLatin1String("peer_skin")) {
        const QString roleId = msg.value(QStringLiteral("roleId")).toString();
        auto it = m_peers.find(roleId);
        if (it != m_peers.end()) {
            it->skin = msg.value(QStringLiteral("skin")).toString();
            emit peersChanged();
        }
        return;
    }
    if (type == QLatin1String("peer_mouse")) {
        const QString roleId = msg.value(QStringLiteral("roleId")).toString();
        auto it = m_peers.find(roleId);
        if (it != m_peers.end()) {
            it->dx = float(msg.value(QStringLiteral("dx")).toDouble());
            it->dy = float(msg.value(QStringLiteral("dy")).toDouble());
            it->eye = msg.value(QStringLiteral("eye")).toInt(0);
            it->es = float(msg.value(QStringLiteral("es")).toDouble());
            emit peersChanged();
        }
        return;
    }
    if (type == QLatin1String("peer_emoticon")) {
        // M4：远端 Tee 表情 → 渲染层在其上方播放
        emit emoticonReceived(msg.value(QStringLiteral("roleId")).toString(),
                              msg.value(QStringLiteral("index")).toInt(0));
        return;
    }
    if (type == QLatin1String("peer_chat")) {
        // 聊天：远端 Tee 文本 → 渲染层在其上方显示气泡
        emit chatReceived(msg.value(QStringLiteral("roleId")).toString(),
                          msg.value(QStringLiteral("text")).toString());
        return;
    }
    if (type == QLatin1String("peer_kicked")) {
        // M4：被房主踢出房间
        emit notify(QStringLiteral("Multiplayer"),
                    QStringLiteral("你已被房主移出房间"), true);
        clearRoom();
        emit roomChanged();
        emit peersChanged();
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
        // rate_limited（如偶发 mouse 撞限流）静默处理：弹模态框会阻塞主线程
        if (code != QLatin1String("rate_limited"))
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
    m_peers.clear();
    m_localRoleReady = false;
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

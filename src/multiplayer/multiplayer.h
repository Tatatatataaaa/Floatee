#ifndef MULTIPLAYER_H
#define MULTIPLAYER_H

#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>
#include "net/netclient.h"

// 联机控制器（M1 连接/房间 + M2 角色/Pers）：封装 NetClient，管理连接状态、
// 房间状态、角色(Peer)表，供托盘菜单与渲染层使用。
class Multiplayer : public QObject
{
    Q_OBJECT
public:
    // 远端角色信息（渲染层据此显示）
    struct PeerInfo {
        QString roleId;
        QString roleName;
        QString skin;
        int hue = 0;          // 皮肤 HSL 调整（随 add_role 同步）
        double sat = 1.0;
        double light = 1.0;
        float dx = 0.0f;      // 鼠标相对其 Tee 中心偏移（眼睛方向）
        float dy = 0.0f;
        int eye = 0;          // 眼睛类型 0..4
        float es = 0.0f;      // 眼睛偏移幅度（0=居中），与本地 eyeScale 一致
    };

    explicit Multiplayer(QObject *parent = nullptr);

    // 初始化身份（clientId 通常用 profile 名，deviceId 首次生成后持久化）
    void init(const QString &clientId, const QString &deviceId, const QString &savedServer);

    // 连接
    void connectTo(const QString &host, quint16 port);
    void disconnect();
    bool isConnected() const { return m_connected; }
    QString serverAddress() const;

    // 房间
    void createRoom(const QString &roomName = QString());
    void joinRoom(const QString &roomId, const QString &joinCode);
    void leaveRoom();
    void listRooms();

    // 本地角色（M2）
    void addLocalRole(const QString &skinName, int hue = 0, double sat = 1.0, double light = 1.0);  // 加入房间后注册本地角色
    void updateLocalSkin(const QString &skinName);  // 皮肤变更上报
    void updateLocalMouse(float dx, float dy, int eye, float es); // 眼睛状态上报（节流）

    // M4 表情 / 管理
    void sendEmoticon(int index);                   // 主动发送表情（服务器限流，rate_limited 静默）
    void kickMember(const QString &clientId);       // 踢出成员（需房主权限，ownerToken 非空）
    void sendChat(const QString &text);             // 发送聊天文本（服务器限流，rate_limited 静默）

    // 状态
    bool inRoom() const { return !m_roomId.isEmpty(); }
    QString roomId() const { return m_roomId; }
    QString joinCode() const { return m_joinCode; }
    QString ownerToken() const { return m_ownerToken; }
    int memberCount() const { return m_peers.size(); }
    QString localRoleId() const { return m_clientId + QStringLiteral("/0"); }
    const QHash<QString, PeerInfo> &peers() const { return m_peers; }

signals:
    // UI 提示（标题/文本/是否警告）
    void notify(const QString &title, const QString &text, bool warn);
    // 连接/房间状态文本（用于托盘 Status 行）
    void statusChanged(const QString &text);
    // 房间状态变化（成员进出/进入离开）
    void roomChanged();
    // 角色表变化（加入/离开/皮肤/眼睛）→ 渲染层刷新
    void peersChanged();
    // M4：收到远端 Tee 的表情（roleId, index）
    void emoticonReceived(const QString &roleId, int index);
    // 收到远端 Tee 的聊天（roleId, text）
    void chatReceived(const QString &roleId, const QString &text);
    // 房间列表查询结果
    void roomListReceived(const QList<QJsonObject> &rooms);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessage(const QJsonObject &msg);
    void onNetError(const QString &e);
    void onReconnectTick();

private:
    void send(const QJsonObject &obj);
    void updateStatus();
    void clearRoom();
    void startReconnect();
    void stopReconnect();
    void upsertPeer(const QJsonObject &member);

    NetClient *m_client = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_heartbeatTimer = nullptr;
    bool m_connected = false;
    bool m_shouldReconnect = false;
    int m_reconnectAttempts = 0;

    QString m_host;
    quint16 m_port = 8764;
    QString m_clientId;
    QString m_deviceId;

    QString m_roomId;
    QString m_joinCode;
    QString m_ownerToken;
    QHash<QString, PeerInfo> m_peers;   // roleId -> PeerInfo（远端角色）
    bool m_localRoleReady = false;      // 收到 role_added 前不上报 mouse/skin（防时序竞争）
    qint64 m_lastMouseSent = 0;
    float m_lastDx = 0.0f, m_lastDy = 0.0f;
    int m_lastEye = -1;
    float m_lastEs = -1.0f;
};

#endif // MULTIPLAYER_H

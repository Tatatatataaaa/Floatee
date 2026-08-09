#ifndef MULTIPLAYER_H
#define MULTIPLAYER_H

#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>
#include "net/netclient.h"

// 联机控制器（M1）：封装 NetClient，管理连接状态、房间状态、成员列表，
// 供托盘 Multiplayer 子菜单与后续渲染层使用。
class Multiplayer : public QObject
{
    Q_OBJECT
public:
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

    // 状态
    bool inRoom() const { return !m_roomId.isEmpty(); }
    QString roomId() const { return m_roomId; }
    QString joinCode() const { return m_joinCode; }
    QString ownerToken() const { return m_ownerToken; }
    int memberCount() const { return m_members.size(); }

signals:
    // UI 提示（标题/文本/是否警告）
    void notify(const QString &title, const QString &text, bool warn);
    // 连接/房间状态文本（用于托盘 Status 行）
    void statusChanged(const QString &text);
    // 房间状态变化（成员进出/进入离开）
    void roomChanged();
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
    QList<QJsonObject> m_members;
};

#endif // MULTIPLAYER_H

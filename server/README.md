# Floatee 联机中转服务器

Floatee 多人联机的公网中转服务器：负责**房间管理**与**消息转发**（纯内存，无数据库）。

## 技术栈

- Node.js ≥ 18（ESM）
- Node 内置 `net`（TCP 服务器，客户端主连接）与 `http`（管理接口）
- `ws`（WebSocket 服务器，备用传输）

## 快速开始

```bash
cd server
npm install
npm start          # 启动（tcp:8764, ws:9001, admin:8766）
npm test           # 单元测试（node:test）
```

配置见 `config.example.json`（复制为 `config.json` 使用；`config.json` 不入库）。
默认值在 `src/config.js`，可用环境变量覆盖：`TCP_PORT`/`PORT`(ws)/`ADMIN_PORT`/`ADMIN_KEY`。

## 传输

| 端口 | 传输 | 协议 | 用途 |
| --- | --- | --- | --- |
| 8764 | TCP | JSON 行（`\n` 分隔） | **客户端主连接**（QTcpSocket） |
| 9001 | WebSocket | JSON 文本帧 | 备用传输（Web 客户端等） |
| 8766 | HTTP | REST | 管理接口（`X-Admin-Key` 鉴权） |

三端共享同一套房间/协议逻辑（`rooms.js` + `protocol.js` + `session.js`）。

## 目录

```
server/
  package.json
  config.example.json   # 配置模板（复制为 config.json 使用；config.json 不入库）
  src/
    index.js     # 入口：装配 ws + tcp + http(admin)
    config.js    # 默认配置 + config.json + env
    protocol.js  # 消息类型常量 + 字段校验
    store.js     # 内存数据模型（Session/Room/Member/Role）
    session.js   # 连接消息分发（握手/心跳/限流/路由）
    rooms.js     # 房间逻辑 + 自动管理 tick
    admin.js     # HTTP 管理接口
    logger.js
  test/          # node:test 单测
```

## 管理接口（HTTP）

> 请求头 `X-Admin-Key: <admin.adminKey>`；`adminKey` 为空则接口禁用。

```text
GET    /api/stats                # 服务器统计
GET    /api/rooms                # 房间列表
GET    /api/rooms/:id            # 房间详情
DELETE /api/rooms/:id            # 强制关闭房间
GET    /api/connections          # 在线连接
DELETE /api/connections/:id      # 强制断开
POST   /api/rooms/:id/broadcast  # 向房间广播 JSON（body）
```

## 协议摘要

见 `../MULTIPLAYER_PLAN.md` §4（与 Qt 客户端 `src/net/protocol.h` 语义一致）：

- C→S：`hello` `create_room` `join_room` `leave_room` `list_rooms` `room_settings` `kick_member` `disband_room` `add_role` `remove_role` `skin_update` `mouse` `emoticon` `chat` `ping`
- S→C：`welcome` `room_created` `room_joined` `room_left` `room_list` `room_settings_updated` `room_closed` `peer_joined` `peer_left` `peer_kicked` `peer_skin` `peer_mouse` `peer_emoticon` `peer_chat` `pong` `error`

要点：默认凭证房（唯一 `joinCode`，随房间存亡）；房主 `ownerToken` 管理（kick/settings/disband）；每设备限 1 连接（`deviceId`）；自动管理 tick（连接超时 30s / 空房 5min 回收 / 房主转移）；事件限流（emoticon 5/s、skin 2/s、mouse 20/s）。

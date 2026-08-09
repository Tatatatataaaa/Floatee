# Floatee 多人联机功能主计划书

> 阶段：下一阶段主计划
> 状态：规划中（未开始实现）
> 日期：2026-08-09
> 目标版本：v0.2（联机）

---

## 1. 概述

为 Floatee 桌面宠物添加**多人联机**能力：

- 一个**公网服务器**作为中转站，负责**创建房间**与**转发数据**；
- 同一房间内的用户会同时看到**其他所有人的 Tee**（支持每人一个或多个 Tee）；
- 服务器同步皮肤数据——**只同步皮肤名**，皮肤文件从各端本地加载，缺失时回落 `default`；
- 提供**主动发送表情**的入口，表情由服务器**实时转发**到房间内其他人；
- 预留**打字交流**等其他功能。

本文档定义总体架构、协议、服务器与客户端实现方案、UI、文件结构、里程碑与风险项，作为本阶段开发的主计划书。

---

## 2. 总体架构

```
                  ┌──────────────────────────────┐
                  │     公网中转服务器 (Server)   │
                  │  - 房间管理 (create/join/leave)│
                  │  - 消息广播/转发              │
                  │  - 心跳/超时/限流            │
                  └──────────────┬───────────────┘
                                 │ WebSocket (JSON)
              ┌──────────────────┼──────────────────┐
              │                  │                  │
     ┌────────▼───────┐ ┌───────▼───────┐ ┌───────▼───────┐
     │ Floatee 客户端A│ │ Floatee 客户端B│ │ Floatee 客户端C│
     │  (多开进程)    │ │  (多开进程)    │ │  (多开进程)    │
     └────────────────┘ └───────────────┘ └───────────────┘
```

- **服务器**只做两件事：**房间管理** + **消息转发**（无业务状态持久化，纯内存；房间数据可放内存）。
- **客户端**（每个 Floatee 进程）连接服务器、加入房间，**仅上报皮肤名与表情事件**（无位置/动画同步）；接收并渲染他人的 Tee（本地自由摆放，详见 §6.4/§8）。
- **每设备限制一个联机进程**（`deviceId` 标识，§4.2/§5.3）：多开进程默认只有一个能联机，避免同一用户在同一房间内被双倍同步。
- 服务器（Node.js）与客户端（Qt C++）**遵循同一份协议规范**（本文档 §4，JSON 为准）：客户端用 C++ 常量、服务器用 JS 常量各自实现，字段名与语义严格一致，避免依赖共享头文件。

---

## 3. 技术选型

| 组件       | 选型                                   | 理由                                                             |
| ---------- | -------------------------------------- | ---------------------------------------------------------------- |
| 客户端网络 | Qt`QWebSocket`（Qt6::WebSockets）    | Qt 原生、跨平台、公网友好（WebSocket over 80/443），天然支持 wss |
| 服务器     | **Node.js (≥18) + `ws`**      | 用户确认；普遍、易维护、无需 Qt 环境；唯一运行时依赖`ws`       |
| 管理接口   | Node 内置`http`（REST）              | 零依赖即可提供手动管理 API（§5.9）                              |
| 消息格式   | **JSON**（UTF-8）                | 可读、可扩展、调试友好；高频状态后续可加紧凑二进制变体           |
| 同步频率   | 状态 10~20Hz 节流                      | 桌面宠物移动频率低，够用且带宽极小                               |
| 房间容量   | 2~8 人（默认 8，`config.json` 可调） | 满足小圈子场景                                                   |

> 客户端 CMake 需新增依赖：`find_package(Qt6 COMPONENTS WebSockets ...)` + `target_link_libraries(... Qt6::WebSockets)`。

---

## 4. 网络协议设计

### 4.1 通用消息封装

所有消息均为单条 JSON 文本：

```json
{ "type": "<消息类型>", "seq": 123, "ts": 1690000000000, ...消息体 }
```

- `type`：见下；
- `seq`：客户端自增序号（可选，用于丢包检测/日志）；
- `ts`：毫秒时间戳（可选）。

### 4.2 身份、设备与角色

- **`clientId`**：连接身份（进程级，用 profile 名/随机串），唯一标识一个连接。
- **`deviceId`**：**设备标识**（首次运行生成 UUID，存配置目录 `device.json`）。服务器按 `deviceId` 限制**每设备一个联机连接**（`security.maxConnsPerDevice=1`）——多开进程加入同一房间导致的"双倍同步"问题由此解决：同一设备第二个联机进程在握手时被拒绝。
- **`roleId`**：角色（Tee）唯一 ID，`clientId + "/" + roleIndex`（如 `A/0`）。v1 每进程 1 个 role；协议预留多 role（将来一人多 Tee）。

### 4.3 消息类型总表

#### 客户端 → 服务器

| type              | 方向 | 说明                                                                                               |
| ----------------- | ---- | -------------------------------------------------------------------------------------------------- |
| `hello`         | C→S | 握手：`{ clientId, deviceId, displayName, version }`（设备限制校验见 §5.3） |
| `create_room`   | C→S | 创建房间：`{ roomName?, capacity?, public? }`（默认凭证房）→ 返回房主令牌 + 唯一邀请码（§5.5） |
| `join_room`     | C→S | 加入房间：`{ roomId, joinCode? }`（公开房可无邀请码，凭证房必须） |
| `leave_room`    | C→S | 离开房间 |
| `list_rooms`    | C→S | 请求房间列表 |
| `room_settings` | C→S | 房主改设置：`{ ownerToken, capacity?, public?, roomName? }` |
| `kick_member`   | C→S | 房主踢人：`{ ownerToken, targetClientId }` |
| `disband_room`  | C→S | 房主解散房间：`{ ownerToken }` |
| `add_role`      | C→S | 注册角色：`{ roleIndex, roleName, skin }` |
| `remove_role`   | C→S | 注销角色：`{ roleIndex }` |
| `skin_update`   | C→S | 切换皮肤：`{ roleId, skin }`（仅变更时上报） |
| `mouse`         | C→S | 眼睛状态：`{ roleId, dx, dy, eye }`（dx/dy=鼠标相对自己 Tee 中心的偏移即眼睛方向，eye=眼睛类型 0..4；节流 10~20Hz） |
| `emoticon`      | C→S | 表情事件：`{ roleId, index }` |
| `ping`          | C→S | 心跳 |

#### 服务器 → 客户端

| type                      | 方向 | 说明                                                                                |
| ------------------------- | ---- | ----------------------------------------------------------------------------------- |
| `welcome`               | S→C | 握手成功：`{ clientId, serverTime, serverVersion }`                               |
| `room_created`          | S→C | 创建成功：`{ roomId, roomName, capacity, public, ownerToken, joinCode }`（§5.5） |
| `room_joined`           | S→C | 加入成功：`{ roomId, roomName, members: [成员摘要] }`                             |
| `room_left`             | S→C | 离开成功                                                                            |
| `room_list`             | S→C | 房间列表：`{ rooms: [{ roomId, roomName, members, capacity, public }] }`          |
| `room_settings_updated` | S→C | 设置变更：`{ roomId, capacity?, public?, roomName? }`                             |
| `room_closed`           | S→C | 房间关闭：`{ roomId, reason }`（解散/空房销毁/管理强制）                          |
| `peer_joined`   | S→C | 有人（role）加入房间：`{ member }`（member 含 skin） |
| `peer_left`     | S→C | 有人（role）离开：`{ roleId }` |
| `peer_kicked`   | S→C | 被房主移出：`{ roomId, reason }`（仅发给被踢者） |
| `peer_skin`     | S→C | 转发他人皮肤变更：`{ roleId, skin }` |
| `peer_mouse`    | S→C | 转发他人眼睛状态：`{ roleId, dx, dy, eye }` |
| `peer_emoticon` | S→C | 转发他人表情：`{ roleId, index }` |
| `pong`          | S→C | 心跳应答 |
| `error`         | S→C | 错误：`{ code, message }` |

### 4.4 消息体字段约定（事件型，无位置/动画同步）

> 本方案**不做位置/walk/缩放同步**（各端 Tee 本地自由摆放，§8），服务器只转发**加入/离开、皮肤名、表情事件**与**眼睛状态**（类型 `eye` + 鼠标偏移 `dx/dy`）；聊天预留。

**角色信息（`add_role` / `peer_joined` 的 member）**

```json
{ "roleId": "A/0", "roleName": "Tata", "skin": "hollowknight.png" }
```

**皮肤变更（`skin_update` / `peer_skin`）**

```json
{ "roleId": "A/0", "skin": "Tata.png" }
```

**表情（`emoticon` / `peer_emoticon`）**

```json
{ "roleId": "A/0", "index": 7 }
```

**眼睛状态（`mouse` / `peer_mouse`）**

```json
{ "roleId": "A/0", "dx": 120.5, "dy": -80.0, "eye": 0 }
```

> 说明：`dx/dy` 为该用户鼠标相对**自己 Tee 画布中心**的偏移（逻辑像素），接收端据此计算远端眼睛的看向方向（与本地眼睛跟随同公式）；`eye` 为眼睛类型（0=Normal..4=Surprise）。两者共同决定远端 Tee 的眼睛外观，**不影响**远端 Tee 位置。

### 4.5 心跳与超时

- 客户端每 **10s** 发 `ping`；服务器回 `pong`。
- 服务器对连接 30s 无消息则断开，并广播 `peer_left`。
- 客户端断线后指数退避重连（1s/2s/4s/…上限 30s），恢复后自动重加入上次房间（v1 可选）。

---

## 5. 服务器设计（Node.js）

### 5.1 技术栈与依赖

- **Node.js ≥ 18**（ESM 模块）；
- **`ws`**：WebSocket 服务器（唯一运行时依赖）；
- 内置 **`http`** 模块提供手动管理接口（REST，不引 Express）；
- 内置 **`node:test`** 做单元测试；
- **无数据库**：全部纯内存，房间生命周期由定时器自动管理。

### 5.2 文件结构

```
server/
  package.json            # name/scripts(start,test)/deps(ws)/engines
  config.json             # 部署配置（覆盖默认值，见 §5.3）
  README.md               # 部署 / 管理 API / 配置说明
  src/
    index.js              # 入口：加载配置、启动 admin(HTTP) + WS、装配模块
    config.js             # 默认配置 + config.json 合并 + 环境变量覆盖
    protocol.js           # 消息类型常量 + 字段校验/长度限制（协议单一定义）
    store.js              # 内存数据模型（connections/rooms/members/roles/tokens）
    session.js            # 单连接：握手、心跳、限流、消息分发、超时
    rooms.js              # 房间逻辑：create/join/leave/disband/kick/settings + 自动管理 tick
    admin.js              # HTTP 管理接口（REST + adminKey 鉴权）
    logger.js             # 结构化日志（时间/级别/事件/roomId/clientId）
  test/
    rooms.test.js         # 房间生命周期 / 凭证 / 权限单测
    protocol.test.js      # 字段校验 / 限流单测
```

### 5.3 配置项（`config.json`，全部有默认值，可用环境变量覆盖）

| 键                          | 默认        | 说明                                           |
| --------------------------- | ----------- | ---------------------------------------------- |
| `server.host`             | `0.0.0.0` | WS 监听地址                                    |
| `server.wsPort`           | `8765`    | WS 端口（env`PORT`）                         |
| `server.adminPort`        | `8766`    | 管理 HTTP 端口（env`ADMIN_PORT`）            |
| `server.maxConnections`   | `1000`    | 全局连接上限                                   |
| `room.capacity`           | `8`       | 每房间默认人数上限                             |
| `room.maxRooms`           | `100`     | 服务器房间总数上限                             |
| `room.roomIdLength`       | `6`       | 房间号长度（字母数字）                         |
| `room.joinCodeLength`     | `8`       | 邀请码长度（字母数字，唯一）                   |
| `room.emptyTtlMs`         | `300000`  | 空房 5 分钟后自动销毁                          |
| `room.idleTimeoutMs`      | `30000`   | 连接 30s 无消息视为超时断开                    |
| `throttle.emoticonPerSec` | `5`       | 表情频率上限                                   |
| `throttle.skinPerSec`     | `2`       | 皮肤变更频率上限                               |
| `throttle.mousePerSec`    | `20`      | 鼠标偏移频率上限（10~20Hz 节流）              |
| `security.maxConnsPerDevice` | `1`    | **每设备最大联机连接数**（解决多开双倍同步）   |
| `security.maxNameLen`     | `32`      | 名字/房间名长度上限                            |
| `security.maxSkinLen`     | `64`      | 皮肤名长度上限                                 |
| `security.maxChatLen`     | `256`     | 聊天长度上限（预留，聊天本期不做）             |
| `security.maxMessageSize` | `8192`    | 单条消息字节上限                               |
| `admin.adminKey`          | `(空)`    | 管理接口鉴权 Key（空则禁用管理接口）           |
| `admin.allowRemote`       | `false`   | 是否允许非本机访问管理接口（默认仅 127.0.0.1） |

### 5.4 数据模型（纯内存）

```js
Store {
  connections: Map<connId, Session>
  rooms:       Map<roomId, Room>
}

Session {                 // 一个 WS 连接
  connId, ws, isAlive,
  authenticated: bool, clientId, deviceId, displayName, version,
  roomId: string|null,
  lastSeen: ts,
  counters: { emoticon: [], skin: [], mouse: [] }   // 限流滑动窗口
}

Room {
  roomId, roomName, capacity, isPublic, createdAt,
  ownerClientId, ownerToken,
  joinCode: string,                 // 唯一邀请码（房间存亡期间有效）
  members: Map<clientId, Member>,
  emptySince: ts|null               // 空房 TTL 起点
}

Member {
  clientId, displayName, joinedAt, isOwner,
  roles: Set<roleId>
}

RoleInfo { roleId, roleName, skin }   // 无位置/动画字段（本地自由摆放）
```

### 5.5 开房令牌与唯一邀请码（核心）

- **create_room**：生成唯一 `roomId`（`roomIdLength` 位）→ 生成
  - `ownerToken`：**房主管理令牌**（唯一，仅返回给创建者；用于 kick/settings/disband，房主转移时重新生成）；
  - `joinCode`：**唯一邀请码**（`joinCodeLength` 位，字母数字，可读、易分享）。
  - 创建者自动成为房主并加入；返回 `room_created { roomId, roomName, capacity, public, ownerToken, joinCode }`。
- **join_room**：
  - 公开房（`public=true`）：无需邀请码；
  - **凭证房（默认）**：必须携带 `joinCode` 与房间的邀请码一致，否则 `error`。
  - 校验房间存在、未满、邀请码正确 → 加入成员 + 创建默认角色 → 广播 `peer_joined` / 返回 `room_joined`。
- **邀请码规则**：
  - **每房间唯一**（创建时生成一个），可**重复使用**（多人用同一邀请码加入），房主通过聊天/外部渠道分享；
  - **生命周期与房间绑定**：房间关闭/销毁/解散时邀请码随之失效，无需短 TTL；
  - 同房间只有一个邀请码，v1 不做批量生成（如需可后续扩展）。

### 5.6 自动管理（房间数据自动方案）

- **全局 tick**（每 10s 一个定时器）：
  - **连接超时**：`lastSeen` 超过 `idleTimeoutMs` → 关闭该连接（清理房间成员、广播 `peer_left`）；
  - **空房回收**：房间成员数为 0 → 记录 `emptySince`，满 `emptyTtlMs` 后销毁并从列表移除（房间内已无人，主要释放资源）；期间有人加入则取消倒计时；
  - （邀请码随房间存亡，无需单独清理。）
- **房主转移**：房主离开/超时/被管理端断开 → 把房主转交给房间内最早加入的成员（重新生成 `ownerToken` 并通知新房主）。
- **解散/关闭**：房主 `disband_room`、管理接口强制关闭、空房到期 → 全员（如有）收到 `room_closed { roomId, reason }`。

### 5.7 房间权限管理（房主）

| 操作              | 需要           | 说明                                                          |
| ----------------- | -------------- | ------------------------------------------------------------- |
| `room_settings` | `ownerToken` | 改容量/公开/房间名，广播`room_settings_updated`             |
| `kick_member`   | `ownerToken` | 移出目标成员（被踢者收`peer_kicked`，他人收 `peer_left`） |
| `disband_room`  | `ownerToken` | 解散房间（全员`room_closed`）                               |
| 普通成员          | —             | 仅 join/leave/emoticon/skin_update/add_role/remove_role        |

- 所有 `ownerToken` 操作先校验令牌归属：与 `room.ownerToken` 严格相等才放行。
- 邀请码（`joinCode`）在创建房间时唯一生成，房主通过外部/聊天渠道分享（§5.5）。

### 5.8 消息处理与限流

- `protocol.js` 对每条消息做**类型 + 长度校验**（含 `maxMessageSize` 检查）；非法 → `error`，不处理。
- **设备限制**：`hello` 时按 `deviceId` 计数，超过 `security.maxConnsPerDevice` 的连接直接拒绝（`error code=device_busy`，客户端提示"本设备已有联机进程"）。
- 每连接**滑动窗口限流**：emoticon 5/s、skin 2/s、mouse 20/s，超限丢弃 + `error` 提示。
- **转发**：校验消息 `roleId` 属于发送者 → 给房间内**其他**客户端广播 `peer_*`。
- 所有字段字符串白名单/长度限制（`security.*`），皮肤名仅接受 `[\w.-]+\.png`。

### 5.9 手动管理接口（HTTP REST，`adminPort`）

> 鉴权：请求头 `X-Admin-Key: <admin.adminKey>`；未配置 `adminKey` 时接口禁用；`allowRemote=false` 时仅本机可访问。

| 方法 路径                         | 说明                                          |
| --------------------------------- | --------------------------------------------- |
| `GET /api/stats`                | 服务器统计（连接数/房间数/消息计数/运行时长） |
| `GET /api/rooms`                | 全部房间（含人数/容量/公开/房主）             |
| `GET /api/rooms/:id`            | 单房间详情（成员、角色、凭证数）              |
| `DELETE /api/rooms/:id`         | 强制关闭房间（成员收`room_closed`）         |
| `GET /api/connections`          | 在线连接列表                                  |
| `DELETE /api/connections/:id`   | 强制断开指定连接                              |
| `POST /api/rooms/:id/broadcast` | 向房间广播自定义消息（运维公告）              |

### 5.10 部署

- 云主机：`npm install` → `npm start`（或 pm2 / systemd / docker）；
- WS 端口 `8765`（公网）；管理端口 `8766` 建议仅本机 + `adminKey`；
- 上线前切 **wss**（反代 443 + 证书）；v1 演示可先 ws；
- 环境变量覆盖：`PORT`、`ADMIN_PORT`、`ADMIN_KEY`。

---

## 6. 客户端设计

### 6.1 模块与文件结构（新增）

```
src/net/
  protocol.h            # 消息类型常量 + JSON 辅助（与服务器 protocol.js 语义一致）
  netclient.h/cpp       # QWebSocket 封装：连接/收发/心跳/重连/信号
src/multiplayer/
  multiplayer.h/cpp     # 联机控制：连接、房间、角色、peers 表、协议分发
  peer.h/cpp            # 远端角色模型（皮肤、本地位置/缩放、表情动画状态）
  peerrenderer.h/cpp    # 远端 Tee 渲染（复用 TeeDrawer/Emoticon 渲染器）
src/ui/
  multiplayermenu 相关    # 托盘 Multiplayer 子菜单（§10）
  emoticon_dial.h/cpp   # 表情圆盘（画布内 overlay，不开第二窗口）
  emoticonsettings.h/cpp # 表情快捷键设置对话框（配置项）
```

### 6.2 网络客户端 NetClient

- 基于 `QWebSocket`：`connectToServer(url)`、`sendJson(QJsonObject)`、`onMessage` 解析。
- 心跳定时器（10s ping）+ 断线信号 + 重连逻辑（指数退避 + 自动重加入）。
- 对外信号：`connected/disconnected/messageReceived`。
- `hello` 携带 `deviceId`（首次运行生成 UUID 存 `device.json`）；被服务器拒绝（`device_busy`）时提示"本设备已有联机进程"。

### 6.3 联机控制器 Multiplayer

职责：

- 持有 `NetClient`；
- 维护自身角色（v1：`roleId = clientId+"/0"`）与**远端角色表** `QHash<QString, Peer>`；
- 订阅本地事件并**上报**：
  - 皮肤变更 → `skin_update`（仅切换时）；
  - **眼睛状态 → `mouse`**（相对本地 Tee 画布中心的鼠标偏移 `dx/dy` + 眼睛类型 `eye`，节流 10~20Hz，用于远端眼睛）；
  - 表情触发 → `emoticon`；
  - **不上报位置/walk/缩放**（各端本地自由摆放，§8）；
- 接收 `peer_joined/peer_left/peer_skin/peer_emoticon` 更新 Peer 表并发出信号给渲染层；
- 皮肤名查找回落（§6.5）；表情转发（§6.6）；
- 断线重连 + 自动重加入最近房间。

### 6.4 多人渲染（核心：全屏透明画布 + 本地自由摆放）

#### 归属层级（问题 2 结论）

**每个联机进程直接管理、渲染它看到的远端 Peer（Peer/PeerRenderer 属于该进程，无"上级联机进程"）**。理由：与"每设备一个联机进程"（§4.2）一致；无需跨进程通信；现有单人渲染架构天然可扩展为进程内多 Peer。

#### 画布

- 联机时主窗口从固定 `192×275` **扩展为全屏透明画布**（**仍是单图层窗口**——规避本机"第二个图层窗口不合成"问题，与现有内容锚定渲染架构一致）。
- 本地 Tee + 所有远端 Peer 的 Tee 都绘制在此画布上。

#### 位置/大小不同步（问题 5 结论）

- 所有 Tee（本地 + 远端）由**本地用户自由拖动摆放**，位置/大小**不向服务器同步**；
- **新加入/重连的 Peer 初始显示在屏幕中心**（默认缩放，与本地 Tee 错开一点）；
- **不做布局持久化**（用户决定）：每次进房间重新摆放即可，退出即丢弃（§8）；
- 无需分辨率适配（各端在各自屏幕本地摆放），也**大幅省流量**（服务器只转发表情/皮肤/眼睛事件）。

#### 动态点击穿透（关键实现）

- 画布窗口默认开启 `WA_TransparentForMouseEvents`（点击穿透，不挡其他程序操作）；
- 16ms 定时器检测光标是否落在**任一 Tee 的包围盒**内：命中 → 临时取消穿透（可交互），否则保持穿透；
- 由此实现"全屏透明画布但不影响其他窗口"。

#### 交互

| 操作 | 行为 |
|---|---|
| 拖拽任一 Tee | 仅改变本地摆放（不广播） |
| 滚轮悬停 Tee | 缩放该 Tee（仅本地显示） |
| 本地 Tee 右键 | **以本地 Tee 中心弹表情圆盘**（不再切眼；眼睛切换改由圆盘内环负责） |
| 远端 Tee 右键 | **独立管理菜单**：隐藏（本地）/ 重置到屏幕中心 / **踢出房间**（需房主权限，`ownerToken`） |
| 眼睛切换 | **走圆盘内环**（本地 CurrentEye；本地右键切眼功能关闭） |
| 本地眼睛 | 仍跟随光标 |
| 远端眼睛 | **同步的眼睛类型 `eye` + 同步鼠标偏移 `dx/dy` 方向**（协议 `peer_mouse`，与本地眼睛同公式） |

#### 渲染

- 每个远端角色一个 **PeerRenderer**：独立 `TeeDrawer`（按皮肤加载，SSAA+羽化复用）+ 独立表情动画状态；
- 绘制顺序：本地 Tee 最后绘制（置顶），其余按 y 排序；
- 未联机时维持现有固定窗口（不扩展全屏）。

#### 角色状态机（Peer）

```
Peer { roleId, roleName, skinName, TeeDrawer* drawer,
       QPointF pos(本地), float scale(本地),
       float mouseDx, mouseDy, int eye, // 远端眼睛状态（方向 + 类型）
       EmoticonState emoticon, qint64 lastSeen }
```

### 6.5 皮肤名同步与回落

- 发送端：仅发送皮肤**文件名**（如 `hollowknight.png`），不传图片字节；加入房间时在 `add_role` 携带一次，切换皮肤时发 `skin_update`。
- 接收端 `TeeDrawer::resolveSkin(name)` 查找顺序：
  1. 内置 qrc 皮肤别名表（`:/skins/<name>`）；
  2. 共享皮肤库 `AppDataLocation/skins/<name>`；
  3. 应用目录 `skins/<name>`；
  4. 全部未命中 → **回落 `defaultSkinPath()`**（并在 UI 提示"对方皮肤不可用，已回落默认"）。
- 服务器只透传皮肤名，不做校验（防注入：接收端只接受 `[\w.-]+\.png` 形式）。

### 6.6 表情入口与转发

- **本地触发**（已有）：拖拽/摸头/切尺寸/周期随机 → 本地显示 + 同时上报 `emoticon`（**切眼不再触发**，眼睛改由圆盘负责）。
- **主动入口（问题 4，沿用 DDNet/QmClient `CEmoticon` 方案 + 交互定制）**：参考 `extracted/EMOTICON_WHEEL.md`，在画布内实现**表情圆盘**（overlay，不开第二窗口）：

  **打开方式（圆盘只作用于本地 Tee）**
  - **右键（主方案）**：在**本地 Tee 上右键** → 以**本地 Tee 中心**为圆心弹出圆盘——右键点即圆心，鼠标天然位于圆盘中心，符合直觉；
  - **热键（附加方案，默认不绑定）**：设置中可配置热键 → 以**本地 Tee 中心**弹出；默认留空避免占用快捷键。
  - 远端 Tee 右键**不弹圆盘**，走独立管理菜单（隐藏/重置/踢出，§6.4 交互表）。

  **几何布局（沿用 DDNet 常量，圆心=目标 Tee 中心）**
  | 常量 | 值 | 含义 |
  | --- | --- | --- |
  | 中心圆 | r=30 | 点击取消/关闭 |
  | 内环（6 眼睛） | item r=70、背景 r=100 | 点击切换眼睛类型 |
  | 外环（16 表情） | item r=150、背景 r=190 | 点击发送表情 |

  **选择逻辑（点击式，区别于 DDNet 的"按住拖动"）**
  - 打开后，以圆盘中心为原点计算鼠标角度/半径：`Index = PositiveMod(round(angle/(2π)*Count), Count)`；
  - 半径分层：≤40 中心（取消）；40~110 内环（眼睛）；>110 外环（表情）；
  - **点击选项即提交**：外层表情 → 本地显示 + 上报 `emoticon`；内层眼睛 → 设置本地 `CurrentEye`（本地行为，不同步）；中心圆 → 关闭。

  **动画（可简化实现）**
  - 弹簧呈现：打开 alpha 0→1 / scale 0.88→1.0，关闭反向（可简化为 QPropertyAnimation）；
  - 渐次 reveal：表情从顶部**顺时针**依次浮现（`EmoticonStaggerReveal`，ease-out quartic）。

  其他主动入口：**数字键** `1`~`0`（可配置映射）；**托盘 Emoticon ▸** 16 项完整列表。

- **配置项**（`default.json`）：`emoticon.wheelKey`（**附加热键，默认空=不绑定**）、`emoticon.wheelEnabled`、`emoticon.rightClickOpen`（默认 true）、`emoticon.numberKeys[10]`；**设置入口**：托盘 **Settings...** 对话框（`emoticonsettings`）。
- **接收端**：收到 `peer_emoticon` → 在对应 Peer 的 Tee 上方播放表情动画（复用 `CEmoticonRenderer`，每 Peer 独立动画状态，2s 生命周期与本地一致）。

### 6.7 聊天（延期，问题 6）

- **本期不实现**；聊天消息类型/入口预留，后续在表情机制基础上扩展（见 §14 待定项）。

---

## 7. 同步频率与带宽估算（事件型，无位置同步）

| 消息          | 频率       | 单条大小 | 说明           |
| ------------ | ---------- | -------- | -------------- |
| `emoticon`   | 事件       | ~60B     | 触发才发       |
| `skin_update`| 事件       | ~60B     | 切换皮肤才发   |
| `mouse`      | 10~20Hz（变化阈值节流） | ~55B | 眼睛状态（类型+方向） |
| `peer_joined/left` | 事件 | ~120B | 加入/离开     |
| `ping`       | 每 10s     | ~40B     | 心跳           |

- 除 `mouse`（远端眼睛状态）外**无持续位置/动画同步** → 带宽很小：8 人房间 `mouse` 广播 ≈ 8×20×55B ≈ 8.8KB/s 上行、广播 ~70KB/s，加上表情/皮肤事件仍毫无压力。
- 服务器限流：emoticon 5/s、skin 2/s、mouse 20/s（§5.3）。

---

## 8. 多人布局（本地自由摆放，不同步）

- **全屏透明画布**：联机时主窗口扩展为全屏，本地与远端 Tee 都在其上（§6.4）。
- **各端本地自由摆放**：每个 Tee 的位置/大小由本地用户拖动/滚轮决定，**不向服务器同步**；服务器只转发表情/皮肤/眼睛状态事件。
- **新加入/重连的 Peer 初始显示在屏幕中心**（默认缩放，与本地 Tee 错开，避免完全重叠）。
- **无需分辨率适配**：各端在各自屏幕本地摆放，天然适配任意分辨率。
- 交互细节见 §6.4 交互表（拖拽/缩放/右键）。

### 8.1 布局持久化：不做

**决定（2026-08-10）**：布局**不做持久化**——每次进房间用户重新摆放即可，退出/断线后布局丢弃。理由：摆放是轻量操作，避免配置文件膨胀与"恢复错位"的维护成本；新 Tee/重连 Tee 一律**默认缩放 + 屏幕中央**。

---

## 9. 健壮性与安全

- **心跳/超时**：30s 无消息断开（§4.5），由服务器自动管理 tick 执行（§5.6）。
- **重连**：指数退避 + 自动重加入（v1 客户端本地保存最近房间的 `roomId` 与 `joinCode`，断线后自动重入）。
- **限流**：表情 5/s、皮肤 2/s（服务器丢弃超额并 `error` 提示，§5.8）。
- **注入防护**：所有字符串字段限制长度（`security.*`）；皮肤名白名单字符集；单消息字节上限。
- **房间准入**：默认凭证房（唯一邀请码 `joinCode`，§5.5），公开房可免码；房主权限由 `ownerToken` 严格校验。
- **TLS**：公网建议 wss（反代 443 + 证书），v1 可先用 ws（演示），上线前切 wss。
- **多开与设备限制**：每设备默认仅 1 个联机连接（`security.maxConnsPerDevice=1`，按 `deviceId` 判定）——多开进程默认只有一个能联机，从机制上避免"同设备多进程加入同一房间导致 Tee 双倍同步"；如需"一人多 Tee"可后续调大该值（同房间内需另行处理去重）。

---

## 10. UI 设计

### 10.1 托盘新增 Multiplayer ▸ 子菜单

```
Multiplayer ▸
  Connect...              # 输入服务器地址（记住上次）
  Disconnect
  ─────────
  Create Room             # 输入房间名（可选）→ 显示房间号 + 邀请码
  Join Room...            # 输入房间号 + 邀请码
  Room List               # 列出当前可加入房间（点选加入）
  Show Join Code          # 复制/展示当前房间邀请码（房主分享用）
  ─────────
  Emoticon ▸              # 16 个表情，点击即本地显示+广播
  Settings...             # 表情快捷键/圆盘设置（§6.6 配置项）
  ─────────
  Status: 离线/已连接/房间 xxx (n/8)
```

### 10.2 状态提示

- 连接状态、房间号、在线人数、邀请码显示在托盘菜单（只读项）与/或窗口角落小标记。
- 皮肤回落提示：收到不可用皮肤名时，在画布角落/托盘提示"对方皮肤不可用，已回落默认"。
- 设备限制提示：第二个联机进程被服务器拒绝时，托盘提示"本设备已有联机进程"。

### 10.3 主动表情入口（重申）

- **表情圆盘**：沿用 DDNet 方案（§6.6）——**在 Tee 上右键打开**（中心=该 Tee 中心）或热键（附加），同心圆（外环 16 表情 + 内环 6 眼睛），**点击选项提交**；
- **右键不再切眼**（眼睛切换走圆盘内环）；
- **数字键** `1`~`0` 快捷发送；
- **托盘 Emoticon ▸** 16 项完整列表；
- 房间外也可本地播放（不广播）。

### 10.4 联机分支的菜单调整

- 联机开发走**新 git 分支**，该分支**屏蔽 WSH（Window Side Hide）与 Eye Care**（对联机冗余）：隐藏对应托盘菜单项、禁用其逻辑，避免全屏画布/边缘吸附冲突。

---

## 11. 项目文件结构（目标形态）

```
Floatee/
├── CMakeLists.txt              # 新增 Qt6::WebSockets
├── MULTIPLAYER_PLAN.md         # 本文档
├── server/                     # 独立 Node.js 服务器（可单独部署）
│   ├── package.json
│   ├── config.json
│   ├── README.md
│   └── src/
│       ├── index.js            # 入口：admin(HTTP) + WS
│       ├── config.js
│       ├── protocol.js         # 协议常量 + 校验（§5.2）
│       ├── store.js            # 内存数据模型
│       ├── session.js          # 连接处理
│       ├── rooms.js            # 房间逻辑 + 自动管理
│       ├── admin.js            # 管理 REST 接口
│       └── logger.js
└── src/
    ├── net/
    │   ├── protocol.h          # 协议常量（与 protocol.js 语义一致）
    │   ├── netclient.h/cpp
    ├── multiplayer/
    │   ├── multiplayer.h/cpp
    │   ├── peer.h/cpp
    │   └── peerrenderer.h/cpp
    └── ui/
        ├── floatee.h/cpp          # 接入联机控制 + 全屏画布渲染
        ├── emoticon_dial.h/cpp    # 表情圆盘（画布内 overlay）
        └── emoticonsettings.h/cpp # 表情快捷键设置对话框
```

---

## 12. 里程碑计划

| 阶段                               | 内容                                                                                                                                                                                                                                                                                                   | 交付物/验收                                                                                           |
| ---------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------- |
| **M0 协议与 Node.js 服务器** | `config.js`/`protocol.js`/`store.js`/`session.js`/`rooms.js`/`admin.js`；连接/握手/心跳、create/join/leave（**含唯一邀请码 joinCode**）、转发、限流、**自动管理 tick（超时/空房回收）**、**房主权限（kick/settings/disband）**、**admin REST**；`node:test` 单测 | `node --test` 通过；服务器日志验证房间生命周期与邀请码校验；curl 验证 admin API；可手动发 JSON 调试 |
| **M1 客户端连接与房间 UI**   | `NetClient`（含 deviceId）+ `Multiplayer` 控制 + 托盘 Connect/Create/Join/Show Code/Room List；重连 | 两客户端连服务器、进出房间，托盘状态正确；第二个联机进程被设备限制拒绝 |
| **M2 多人渲染**              | 全屏透明画布 + **动态点击穿透** + Peer 渲染 + 本地自由摆放（拖拽/缩放/初始屏幕中心）+ **眼睛状态同步（mouse/peer_mouse）** | 同房间两台机器互相看到对方 Tee（各端本地摆放）；远端眼睛跟随其鼠标；画布不挡其他窗口 |
| **M3 皮肤同步**              | 皮肤名上报（add_role/skin_update）/接收/本地查找/回落 default | 不同皮肤互相正确显示；缺失皮肤回落提示 |
| **M4 表情转发**              | 表情圆盘 + 数字键 + 托盘 Emoticon；广播 + 远端动画 | 一方发表情，他人对应 Tee 上方实时播放 |
| **M5 设置与打磨**            | 表情快捷键设置对话框、wss、状态提示完善 | 完整联机体验，文档/部署脚本 |

> 聊天本期不做（延期，见 §6.7）；房间密码等其他能力见 §14 待定项。

> 每个里程碑结束同步更新本计划书状态与 PROGRESS.md。

---

## 13. 风险与待定项

| 项                                | 影响                      | 对策                                                                   |
| --------------------------------- | ------------------------- | ---------------------------------------------------------------------- |
| 第二个图层窗口在本机不合成        | 多窗口方案不可行          | 采用**单窗口全屏画布**（§6.4），全部 Tee 一个透明窗口绘制              |
| 全屏画布点击穿透                  | 需交互与穿透兼顾          | **动态穿透**：定时器检测光标是否在 Tee 包围盒内，命中才取消穿透（§6.4）|
| 性能（多 Tee 同时渲染+羽化+SSAA） | 每帧多角色渲染开销        | Peer 渲染按需（静止/不可见时不重渲染）；限制同屏角色数；羽化可按需降级 |
| 服务器公网稳定性                  | 掉线                     | 心跳+重连+自动重加入；布局本地持久化                                   |
| 皮肤注入/字段滥用                 | 安全                      | 白名单+长度限制+限流                                                   |
| 多开导致同房间双倍同步            | 同一设备多进程入同房间    | **按 deviceId 限制每设备 1 个联机连接**（`security.maxConnsPerDevice`）；如需一人多 Tee 再放开并做去重 |

### 13.1 客户端交互定稿与待确认项

**已定稿（2026-08-10）**

- [x] **表情圆盘**：沿用 DDNet/QmClient `CEmoticon` 方案（`extracted/EMOTICON_WHEEL.md`）——同心圆（外环 16 表情 + 内环 6 眼睛）、角度+半径分层、弹簧+渐次 reveal 动画；**交互定制**：右键在**本地 Tee** 上打开（中心=本地 Tee 中心）+ 点击式提交，热键作附加（默认不绑定）；**右键切眼功能关闭**（眼睛走圆盘内环）；**远端 Tee 右键走独立管理菜单**（隐藏/重置/踢出[需房主权限]）（§6.4/§6.6）；
- [x] **远端眼睛**：**同步眼睛状态**（协议 `mouse`/`peer_mouse`，`dx/dy` 鼠标偏移 + `eye` 眼睛类型，节流 10~20Hz），远端眼睛 = 同步类型 + 同步方向（§4.4/§6.4）；
- [x] **WSH 与 Eye Care**：联机开发走**新 git 分支**，在该分支**屏蔽这两个功能**（对联机冗余）；
- [x] **第二联机进程**：不做本地检测，用户点击联机菜单被服务器拒绝（`device_busy`）时反馈提示即可。

**待确认**

- [ ] ~~表情圆盘附加热键~~：**已定**——默认不绑定（避免占用快捷键），用户在设置中自行配置。

---

## 14. 附录：关键消息示例

**创建房间（返回房主令牌 + 唯一邀请码）**

```json
→ { "type":"create_room", "roomName":"friends", "capacity":8 }
← { "type":"room_created", "roomId":"A3fK2q", "roomName":"friends", "capacity":8,
    "public":false,
    "ownerToken":"a1b2...", "joinCode":"XY7mQ2pK" }
```

**加入房间（凭证房，服务器向全员广播）**

```json
→ { "type":"join_room", "roomId":"A3fK2q", "joinCode":"XY7mQ2pK" }
← (加入者) { "type":"room_joined", "roomId":"A3fK2q", "roomName":"friends",
             "members":[{"roleId":"A/0","roleName":"Tata","skin":"Tata.png"}] }
← (他人)   { "type":"peer_joined",
             "member":{"roleId":"B/0","roleName":"blue","skin":"hollowknight.png"} }
```

**房主踢人**

```json
→ { "type":"kick_member", "ownerToken":"a1b2...", "targetClientId":"B" }
← (被踢) { "type":"peer_kicked", "roomId":"A3fK2q", "reason":"kicked" }
← (他人) { "type":"peer_left", "roleId":"B/0" }
```

**管理接口（HTTP）**

```text
GET    /api/rooms
DELETE /api/rooms/A3fK2q          # 强制关闭
GET    /api/stats
Header: X-Admin-Key: <admin.adminKey>
```

**皮肤变更**

```json
→ { "type":"skin_update", "roleId":"A/0", "skin":"Tata.png" }
← (他人收到) { "type":"peer_skin", "roleId":"A/0", "skin":"Tata.png" }
```

**表情**

```json
→ { "type":"emoticon", "roleId":"A/0", "index":7 }
← (他人收到) { "type":"peer_emoticon", "roleId":"A/0", "index":7 }
```

**眼睛状态（远端眼睛）**

```json
→ { "type":"mouse", "roleId":"A/0", "dx":120.5, "dy":-80.0, "eye":0 }
← (他人收到) { "type":"peer_mouse", "roleId":"A/0", "dx":120.5, "dy":-80.0, "eye":0 }
```

**心跳**

```json
→ { "type":"ping" }
← { "type":"pong" }
```

---

*下一步：确认本计划书 → 开始 M0（协议 + 服务器骨架）。*

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
- **客户端**（每个 Floatee 进程）连接服务器、加入房间、上报自身状态（位置/动画/皮肤名等）、接收并渲染他人状态。
- 服务器与客户端**共享同一份协议定义**（`protocol.h`），降低两端不一致风险。

---

## 3. 技术选型

| 组件 | 选型 | 理由 |
|---|---|---|
| 客户端网络 | Qt `QWebSocket`（Qt6::WebSockets） | Qt 原生、跨平台、公网友好（WebSocket over 80/443），天然支持 wss |
| 服务器 | **Qt C++ `QWebSocketServer`**（首选） | 与客户端同语言，可直接复用 `protocol.h`；事件驱动、轻量 |
| 服务器（备选） | Node.js + `ws` | 若希望独立部署/更轻量亦可，但需复制协议定义 |
| 消息格式 | **JSON**（UTF-8） | 可读、可扩展、调试友好；高频状态后续可加紧凑二进制变体 |
| 同步频率 | 状态 10~20Hz 节流 | 桌面宠物移动频率低，够用且带宽极小 |
| 房间容量 | 2~8 人（v1 上限 8，可配置） | 满足小圈子场景 |

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

### 4.2 身份与角色（一个或多个 Tee）

- 每个**连接**= 一个客户端进程（可为多开实例，用其 profile 作为 `clientId` 来源之一）。
- 每个客户端可注册**一个或多个角色（role）**，每个 role 是一个 Tee（即"一个或多个 Tee"的来源，也兼容将来一人多角色）。
- 角色全局唯一 ID：`clientId + "/" + roleIndex`（如 `A/0`、`A/1`）。
- v1 每个进程默认注册 1 个 role；协议预留多 role，不限制死。

### 4.3 消息类型总表

#### 客户端 → 服务器

| type | 方向 | 说明 |
|---|---|---|
| `hello` | C→S | 握手：`{ clientId, displayName, version }` |
| `create_room` | C→S | 创建房间：`{ roomName?, capacity? }` |
| `join_room` | C→S | 加入房间：`{ roomId }` |
| `leave_room` | C→S | 离开房间 |
| `list_rooms` | C→S | 请求房间列表 |
| `add_role` | C→S | 注册角色：`{ roleIndex, roleName }` |
| `remove_role` | C→S | 注销角色：`{ roleIndex }` |
| `state` | C→S | 角色状态：`{ roleId, skin, posX, posY, eye, dirX, dirY, walkPhase, scale }` |
| `emoticon` | C→S | 表情事件：`{ roleId, index, seq? }` |
| `chat` | C→S | 聊天文本：`{ text }` |
| `ping` | C→S | 心跳 |

#### 服务器 → 客户端

| type | 方向 | 说明 |
|---|---|---|
| `welcome` | S→C | 握手成功：`{ clientId, serverTime, serverVersion }` |
| `room_created` | S→C | 创建成功：`{ roomId, roomName }` |
| `room_joined` | S→C | 加入成功：`{ roomId, roomName, members: [成员摘要] }` |
| `room_left` | S→C | 离开成功 |
| `room_list` | S→C | 房间列表：`{ rooms: [{ roomId, roomName, members, capacity }] }` |
| `peer_joined` | S→C | 有人（role）加入房间：`{ member }` |
| `peer_left` | S→C | 有人（role）离开：`{ roleId }` |
| `peer_state` | S→C | 转发他人状态（同 `state` 体） |
| `peer_emoticon` | S→C | 转发他人表情（同 `emoticon` 体） |
| `peer_chat` | S→C | 转发聊天：`{ clientId, displayName, text, ts }` |
| `pong` | S→C | 心跳应答 |
| `error` | S→C | 错误：`{ code, message }` |

### 4.4 消息体字段约定

`state` 与 `peer_state` 共享角色状态结构：

```json
{
  "type": "state",
  "roleId": "A/0",
  "skin": "hollowknight.png",
  "posX": 1240.5, "posY": 680.0,   // 房间坐标（详见 §8）
  "eye": 0,                          // 0=Normal..4=Surprise（与本地一致）
  "dirX": 0.7, "dirY": -0.7,         // 眼睛看向方向
  "walkPhase": 0.25,                 // walk 相位 [0,1)，-1=idle
  "scale": 1.0                       // 角色缩放
}
```

### 4.5 心跳与超时

- 客户端每 **10s** 发 `ping`；服务器回 `pong`。
- 服务器对连接 30s 无消息则断开，并广播 `peer_left`。
- 客户端断线后指数退避重连（1s/2s/4s/…上限 30s），恢复后自动重加入上次房间（v1 可选）。

---

## 5. 服务器设计

### 5.1 模块

```
server/
  CMakeLists.txt
  src/main.cpp            # 启动、端口、日志
  src/protocol.h          # 与客户端共享的消息类型/常量
  src/room.h/cpp          # 房间：成员/角色表、广播
  src/session.h/cpp       # 单连接：握手、心跳、消息分发
```

### 5.2 数据模型（纯内存）

```
Server
 └─ unordered_map<clientId, Session>   // 在线连接
 └─ unordered_map<roomId, Room>        // 房间
Room
 ├─ roomId (短随机串，如 6 位字母数字)
 ├─ roomName
 ├─ capacity (默认 8)
 └─ roles: unordered_map<roleId, RoleInfo>
      RoleInfo { clientId, roleIndex, roleName, skin, posX, posY, eye, ... }
```

### 5.3 核心逻辑

- **create_room**：生成唯一 `roomId`；发送者自动成为房主并加入；返回 `room_created`。
- **join_room**：校验房间存在且未满；加入；给房间内所有人广播 `peer_joined`，给加入者 `room_joined`（含现有成员摘要）。
- **leave / 断开**：从房间移除该客户端所有 role，广播 `peer_left`；房主离开时把房主转交给剩余成员（v1：房间保留，任意成员离开均不销毁房间，空房超时销毁）。
- **state/emoticon/chat 转发**：校验 `roleId` 属于发送者 → 广播给房间内**其他**客户端（`peer_*`）。
- **限流**：单客户端状态消息频率上限（如 50/s），超出丢弃并警告，防刷屏。
- **空房回收**：房间空置 5 分钟无成员自动销毁。

### 5.4 部署

- 公网服务器监听 `0.0.0.0:PORT`（默认 8765，或走 wss + 反代 443）。
- 服务器地址与端口在客户端配置（默认值编译进程序，可在配置文件中覆盖）。
- 部署建议：云主机 + systemd / docker；v1 直接裸进程。

---

## 6. 客户端设计

### 6.1 模块与文件结构（新增）

```
src/net/
  protocol.h            # 消息类型常量 + JSON 辅助（客户端与服务器共享）
  netclient.h/cpp       # QWebSocket 封装：连接/收发/心跳/重连/信号
src/multiplayer/
  multiplayer.h/cpp     # 联机控制：连接、房间、角色、peers 状态表、协议分发
  peer.h/cpp            # 远端角色模型（皮肤、位置、动画状态、表情）
  peerrenderer.h/cpp    # 远端 Tee 渲染（复用 TeeDrawer/Emoticon 渲染器）
src/ui/
  multiplayer_menu 相关   # 托盘 Multiplayer 子菜单（见 §10）
  chatwindow.h/cpp      # 聊天窗口（M5）
```

### 6.2 网络客户端 NetClient

- 基于 `QWebSocket`：`connectToServer(url)`、`sendJson(QJsonObject)`、`onMessage` 解析。
- 心跳定时器（10s ping）+ 断线信号 + 重连逻辑。
- 对外信号：`connected/disconnected/messageReceived`。

### 6.3 联机控制器 Multiplayer

职责：

- 持有 `NetClient`；
- 维护自身角色（v1：`roleId = clientId+"/0"`）与**远端角色表** `QHash<QString, Peer>`；
- 订阅本地事件并**上报**：
  - 本地 Tee 位置/眼睛/walk/皮肤/缩放变化（节流）→ `state`；
  - 本地表情触发 → `emoticon`；
- 接收 `peer_*` 消息更新 Peer 表并发出信号给渲染层；
- 皮肤名查找回落（§6.5）；
- 表情转发（§6.6）。

### 6.4 多人渲染（核心）

#### 方案：同屏世界视图（v1 推荐）

- 房间内每个人共享一个**虚拟房间坐标系**（单位：逻辑像素，约定各端屏幕分辨率相近即可，v1 不做分辨率归一化）。
- **本地 Tee**：沿用现有窗口内 `m_teePos` 渲染；本地拖动 → 换算为房间坐标变化 → 节流上报 `state`。
- **远端 Tee**：屏幕位置 = 本地窗口原点 + (远端房间坐标 − 本地房间坐标)，绘制在**主窗口之外额外扩展的区域**：

  为容纳远端 Tee，主窗口从固定 `192×275` **扩展为更大的世界画布**（例如 `max(本地窗口, 需要显示所有 Tee 的包围盒)`），或使用**独立的多人场景窗口**。

  > 两种子方案（M2 细化后选一）：
  > - **A. 单窗口扩展**：一个透明窗口容纳所有 Tee（本地 + 远端），窗口尺寸 = 所有可见 Tee 的包围盒 + 边距；本地 Tee 仍固定中心语义，滚动/拖拽切换。优点：仍是单个图层窗口（本机图层窗口合成可靠，见历史结论），实现直接。
  > - **B. 主窗口 + 远端覆盖层**：主窗口保持本地，远端 Tee 画进同一主窗口的放大画布。实现上就是 A。
  >
  > **结论：采用 A（单窗口、世界画布、所有 Tee 一起绘制）**，与现有"内容锚定"渲染架构一致，避免第二个图层窗口（本机不可合成）。

- 每个远端角色一个 **PeerRenderer**：持有独立 `TeeDrawer`（按皮肤加载）与表情渲染状态，渲染到世界画布。
- **位置平滑**：远端 Tee 位置用线性插值（lerp，系数 ~0.2/帧）或短滑动平均，避免跳帧；眼睛方向/walk 相位直接取最新（低风险）。
- **绘制顺序**：本地 Tee 最后绘制（置顶，或按 y 排序，后画者在上）。

#### 角色状态机（Peer）

```
Peer { roleId, roleName, skinName, TeeDrawer* drawer, QPointF pos,
       QPointF targetPos, int eye, QPointF dir, float walkPhase,
       EmoticonState emoticon, qint64 lastUpdate }
```

### 6.5 皮肤名同步与回落

- 发送端：仅发送皮肤**文件名**（如 `hollowknight.png`），不传图片字节。
- 接收端 `TeeDrawer::resolveSkin(name)` 查找顺序：
  1. 内置 qrc 皮肤别名表（`:/skins/<name>`）；
  2. 共享皮肤库 `AppDataLocation/skins/<name>`；
  3. 应用目录 `skins/<name>`；
  4. 全部未命中 → **回落 `defaultSkinPath()`**（并在 UI 提示"对方皮肤不可用，已回落默认"）。
- 服务器只透传皮肤名，不做校验（防注入：接收端只接受 `[\w.-]+\.png` 形式）。

### 6.6 表情入口与转发

- **本地触发**（已有）：拖拽/摸头/切眼/切尺寸/周期随机 → 本地显示 + 同时上报 `emoticon`。
- **新增主动入口**：托盘新增 **Emoticon ▸** 子菜单（16 个表情，与 `emoticons.png` 4×4 网格一致），点击 → 本地立即显示 + 广播。
- **接收端**：收到 `peer_emoticon` → 在对应 Peer 的 Tee 上方播放表情动画（复用 `CEmoticonRenderer`，每个 Peer 一个独立动画状态，2s 生命周期与本地一致）。
- 表情消息为**事件型**，无需节流（但服务器限流防刷）。

### 6.7 聊天（M5，预留）

- `peer_chat` 转发文本；客户端底部/独立小窗显示气泡列表。
- 输入框入口：托盘 Multiplayer ▸ Chat，或主窗口下方折叠条。

---

## 7. 同步频率与带宽估算

| 消息 | 频率 | 单条大小 | 说明 |
|---|---|---|---|
| `state` | 10~20Hz（变化节流：移动时 20Hz，静止时停止发送） | ~150B | 位置/眼睛/walk |
| `emoticon` | 事件 | ~60B | 触发才发 |
| `chat` | 事件 | ~200B | 用户输入 |
| `ping` | 每 10s | ~40B | 心跳 |

- 8 人房间最坏 ~8×20×150B ≈ 24KB/s 上行，服务器广播 ~24KB/s×8，公网带宽完全无压力。
- 状态节流策略：位置移动超过阈值（如 0.5px）才发；静止 1s 后完全停止发送。

---

## 8. 房间坐标与多人布局（细化）

- **虚拟房间坐标系**：约定为"各端屏幕坐标的直接映射"（v1 不做分辨率归一化）。本地 Tee 的房间坐标 = 本地屏幕坐标；远端 Tee 显示偏移 = 远端房间坐标 − 本地房间坐标。
- **初始分布**：加入房间时，服务器分配角色槽位，客户端按槽位在本地世界画布中放置远端 Tee 的**初始相对偏移**（如围绕本地 Tee 环形分布），随后由位置同步接管。
- 本地拖动 Tee → 更新自身房间坐标 → 上报 → 他人看到本地 Tee 移动。
- 缩放只影响本地显示（`scale` 同步给他人作为参考；他人渲染时按比例放大对方 Tee 即可，v1 可先固定按对方 scale 渲染）。

---

## 9. 健壮性与安全

- **心跳/超时**：30s 无消息断开（§4.5）。
- **重连**：指数退避 + 自动重加入。
- **限流**：状态 50/s、表情 5/s、聊天 5/s（服务器丢弃超额并 `error` 提示）。
- **注入防护**：所有字符串字段限制长度（如名字 ≤32、皮肤名 ≤64）；皮肤名白名单字符集。
- **房间密码**（可选，M5）：`join_room` 带密码字段，v1 可不做。
- **TLS**：公网建议 wss（服务器配证书或反代），v1 可先用 ws（演示），上线前切 wss。
- **多开兼容**：每个 Floatee 进程独立连服务器（profile 不同 → clientId 不同），互不干扰；同一房间内多开进程可互相看到（即"一个用户多个 Tee"的一种实现：多开即多角色）。

---

## 10. UI 设计

### 10.1 托盘新增 Multiplayer ▸ 子菜单

```
Multiplayer ▸
  Connect...              # 输入服务器地址（记住上次）
  Disconnect
  ─────────
  Create Room             # 输入房间名（可选）→ 显示房间号
  Join Room...            # 输入房间号
  Room List               # 列出当前可加入房间（点选加入）
  ─────────
  Emoticon ▸              # 16 个表情，点击即本地显示+广播（主动表情入口）
  Chat...                 # 聊天窗口（M5）
  ─────────
  Status: 离线/已连接/房间 xxx (n/8)
```

### 10.2 状态提示

- 连接状态、房间号、在线人数显示在托盘菜单（只读项）与/或窗口角落小标记。
- 皮肤回落提示：收到不可用皮肤名时，在聊天区/托盘提示。

### 10.3 主动表情入口（重申）

托盘 **Multiplayer ▸ Emoticon ▸** 16 项，选中立即本地播放 + 广播；房间外也可点（仅本地播放）。

---

## 11. 项目文件结构（目标形态）

```
Floatee/
├── CMakeLists.txt              # 新增 Qt6::WebSockets
├── MULTIPLAYER_PLAN.md         # 本文档
├── server/                     # 独立服务器工程（可单独构建/部署）
│   ├── CMakeLists.txt
│   └── src/
│       ├── protocol.h          # 与客户端共享
│       ├── main.cpp
│       ├── room.h/cpp
│       └── session.h/cpp
└── src/
    ├── net/
    │   ├── protocol.h          # 共享协议
    │   ├── netclient.h/cpp
    ├── multiplayer/
    │   ├── multiplayer.h/cpp
    │   ├── peer.h/cpp
    │   └── peerrenderer.h/cpp
    └── ui/
        ├── floatee.h/cpp       # 接入联机控制 + 世界画布渲染
        └── chatwindow.h/cpp    # M5
```

---

## 12. 里程碑计划

| 阶段 | 内容 | 交付物/验收 |
|---|---|---|
| **M0 协议与服务器骨架** | `protocol.h`；服务器：连接/握手/心跳、create/join/leave/list、转发、限流 | 服务器命令行日志验证房间生命周期；可手动发 JSON 调试 |
| **M1 客户端连接与房间 UI** | `NetClient` + `Multiplayer` 控制 + 托盘 Connect/Create/Join/Room List；重连 | 两客户端连服务器、进出房间，托盘状态正确 |
| **M2 多人渲染** | 世界画布 + Peer 渲染 + 位置同步/平滑；多 Tee 同屏 | 同房间两台机器互相看到对方 Tee 移动/眼睛跟随 |
| **M3 皮肤同步** | 皮肤名上报/接收/本地查找/回落 default | 不同皮肤互相正确显示；缺失皮肤回落提示 |
| **M4 表情转发** | 主动表情入口 + 广播 + 远端动画 | 一方发表情，他人对应 Tee 上方实时播放 |
| **M5 聊天与打磨** | 聊天窗口、房间密码（可选）、wss、状态提示完善 | 完整联机体验，文档/部署脚本 |

> 每个里程碑结束同步更新本计划书状态与 PROGRESS.md。

---

## 13. 风险与待定项

| 项 | 影响 | 对策 |
|---|---|---|
| 第二个图层窗口在本机不合成 | 多窗口方案不可行 | 采用**单窗口世界画布**方案（§6.4-A），全部 Tee 一个透明窗口绘制 |
| 各端屏幕分辨率/布局不一致 | 房间坐标 = 屏幕坐标会错位 | v1 接受；后续可加坐标归一化（按屏幕尺寸比例） |
| 性能（多 Tee 同时渲染+羽化+SSAA） | 每帧多角色渲染开销 | Peer 渲染按需（静止时不重渲染）；限制同屏角色数；羽化可按需降级 |
| 服务器公网稳定性 | 掉线/丢包 | 心跳+重连+自动重加入；状态用最后已知值插值 |
| 皮肤注入/字段滥用 | 安全 | 白名单+长度限制+限流 |
| 多开与角色映射 | 一人多 Tee 语义 | roleId 唯一化；多开进程=多连接=多角色 |

---

## 14. 附录：关键消息示例

**创建房间**
```json
→ { "type":"create_room", "roomName":"friends", "capacity":8 }
← { "type":"room_created", "roomId":"A3fK2q", "roomName":"friends" }
```

**加入房间（服务器向全员广播）**
```json
→ { "type":"join_room", "roomId":"A3fK2q" }
← (加入者) { "type":"room_joined", "roomId":"A3fK2q", "roomName":"friends",
             "members":[{"roleId":"A/0","roleName":"Tata","skin":"Tata.png"}] }
← (他人)   { "type":"peer_joined",
             "member":{"roleId":"B/0","roleName":"blue","skin":"hollowknight.png"} }
```

**状态同步**
```json
→ { "type":"state", "roleId":"A/0", "skin":"Tata.png",
    "posX":1200,"posY":640,"eye":0,"dirX":0.5,"dirY":-0.9,"walkPhase":-1,"scale":1.0 }
← (他人收到) { "type":"peer_state", ... 同上体 ... }
```

**表情**
```json
→ { "type":"emoticon", "roleId":"A/0", "index":7 }
← (他人收到) { "type":"peer_emoticon", "roleId":"A/0", "index":7 }
```

**聊天**
```json
→ { "type":"chat", "text":"hi all" }
← { "type":"peer_chat", "clientId":"B", "displayName":"blue", "text":"hi all", "ts":1690000000000 }
```

**心跳**
```json
→ { "type":"ping" }
← { "type":"pong" }
```

---

*下一步：确认本计划书 → 开始 M0（协议 + 服务器骨架）。*

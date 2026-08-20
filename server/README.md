# Floatee 联机中转服务器

Floatee 多人联机的公网中转服务器：负责**房间管理**与**消息转发**（纯内存，无数据库）。

## 技术栈

- Node.js ≥ 18（ESM）
- Node 内置 `net`（TCP 服务器，客户端主连接）与 `http`（管理接口）
- `ws`（WebSocket 服务器，备用传输，已不再使用）

## 环境要求

| 项目 | 要求 |
|---|---|
| Node.js | **≥ 18**（`npm` 随附） |
| 操作系统 | Windows / Linux / macOS 均可 |
| 网络 | 需能被客户端访问（局域网或公网） |

检查版本：

```bash
node -v   # 应 ≥ v18
npm -v
```

## 安装与依赖

```bash
cd server
npm install        # 安装唯一依赖 ws
```

依赖极少（仅 `ws`），无需编译。

## 快速开始

```bash
cd server
npm install
npm start          # 启动（tcp:9000, admin:9001）
npm test           # 单元测试（node:test）
```

配置见 `config.example.json`（复制为 `config.json` 后修改）
可用环境变量覆盖：`TCP_PORT`/`ADMIN_PORT`/`ADMIN_KEY`

### 修改端口（端口被占用时）

直接编辑 `config.json`（不入库，从 `config.example.json` 复制）中的 `server` 段，改 `tcpPort` 与 `adminPort` 后重启即可：

```jsonc
{
  "server": {
    "tcpPort": 9000,     // 客户端主连接（TCP）
    "adminPort": 9001    // 管理接口（HTTP）
  }
}
```

也可以用环境变量覆盖（优先级高于 config.json）：

```bash
TCP_PORT=9100 ADMIN_PORT=9101 node src/index.js
```

> 客户端连接时需使用对应的 TCP 端口（`host:tcpPort`），Admin 端口仅供管理接口使用。

## 启动服务器

```bash
# 前台启动（调试用）
npm start
# 或
node src/index.js

# 后台启动（Linux/macOS，日志写入文件）
nohup node src/index.js > floatee.log 2>&1 &

# 后台启动（Windows PowerShell）
Start-Process -FilePath node -ArgumentList "src/index.js" -WindowStyle Hidden
```

启动成功日志示例：

```
floatee_server_started { wsPort: 9002, tcpPort: 9000, adminPort: 9001, ... }
tcp_listening { host: '0.0.0.0', port: 9000 }
```

## 传输

| 端口 | 传输 | 协议                   | 用途                                 |
| ---- | ---- | ---------------------- | ------------------------------------ |
| 9000 | TCP  | JSON 行（`\n` 分隔） | **客户端主连接**（QTcpSocket） |
| 9001 | HTTP | REST                   | 管理接口（`X-Admin-Key` 鉴权）     |

> WebSocket 已不再使用；服务器仍监听 `wsPort`（默认 9002）仅为兼容旧配置，客户端不使用。

共享同一套房间/协议逻辑（`rooms.js` + `protocol.js` + `session.js`）

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

## 配置

配置优先级：**默认值 < `config.json` < 环境变量**。

### 配置文件 `config.json`

> **安全提示**：`config.json` 含敏感信息（`adminKey`），**不入库**（已加入 `server/.gitignore`）。
> 部署时复制 `config.example.json` 为 `config.json` 后修改；Admin 密钥建议通过环境变量
> `ADMIN_KEY` 提供（`ADMIN_KEY=... node src/index.js`）。若密钥曾提交到公开仓库，应立即**轮换**。

```jsonc
{
  "server": {
    "host": "0.0.0.0",        // 监听地址：0.0.0.0=所有网卡（局域网/公网）
    "wsPort": 9002,           // WebSocket（已不再使用，兼容旧配置）
    "tcpPort": 9000,          // TCP 客户端端口
    "adminPort": 9001,        // Admin HTTP 端口
    "maxConnections": 1000    // 最大连接数
  },
  "room": {
    "capacity": 8,            // 每房间最大成员
    "maxRooms": 100,          // 最大房间数
    "maxPublicRooms": 8,      // 公共房间上限（0 = 不限制）
    "roomIdLength": 6,        // 房间号长度（纯数字）
    "joinCodeLength": 4,      // 邀请码长度（大写字母+数字）
    "emptyTtlMs": 300000,     // 空房 5 分钟自动销毁
    "idleTimeoutMs": 30000    // 连接 30s 无消息判定超时断开
  },
  "throttle": {
    "emoticonPerSec": 5,      // 表情频率上限/秒
    "skinPerSec": 2,          // 皮肤切换上限/秒
    "mousePerSec": 20         // 鼠标状态上限/秒（客户端 60ms 节流≈16.7/s）
  },
  "security": {
    "maxConnsPerDevice": 1,   // 每设备最多 1 个联机进程
    "maxIdLen": 64,
    "maxSkinLen": 64,
    "maxMessageSize": 8192
  },
  "admin": {
    "adminKey": "",           // 留空 = 禁用 Admin 接口
    "allowRemote": false      // 是否允许远程访问 Admin
  }
}
```

### 环境变量（覆盖 config.json）

| 变量 | 作用 |
|---|---|
| `TCP_PORT` | 覆盖 TCP 端口（9000） |
| `ADMIN_PORT` | 覆盖 Admin 端口（9001） |
| `ADMIN_KEY` | 覆盖 Admin 鉴权密钥 |

```bash
TCP_PORT=9100 ADMIN_PORT=9101 ADMIN_KEY=secret node src/index.js
```

### 修改后生效

修改 `config.json` 后需**重启服务器**：

```bash
# 先停旧进程
# Linux: 找到进程 kill，或用 pkill -f "src/index.js"
# Windows: 任务管理器结束 node.exe，或 taskkill /IM node.exe /F
# 再启动
npm start
```

> 注意：Windows 上端口被占用会报 `EADDRINUSE`，先结束旧 node 进程再启动。

## 部署场景

### 局域网部署（最简单）

服务器与客户端在同一局域网（如家用 WiFi）：

1. 任一台机器运行服务器（`0.0.0.0` 监听）
2. **Windows 防火墙放行 TCP 9000**（以及 9001，如开启 Admin 远程访问）：
   ```powershell
   # 管理员 PowerShell
   netsh advfirewall firewall add rule name="Floatee TCP 9000" dir=in action=allow protocol=TCP localport=9000
   netsh advfirewall firewall add rule name="Floatee TCP 9001" dir=in action=allow protocol=TCP localport=9001
   ```
3. 客户端在"Multiplayer → Connect..."中输入 `服务器IP:9000`（如 `192.168.10.33:9000`）

### 公网部署（云服务器）

1. 云服务器安全组/防火墙放行 **9000**（TCP）、**9001**（TCP，可选，Admin 远程）
2. 建议 `maxConnsPerDevice` 保持 1，`adminKey` 设置强密码并开启 `allowRemote`（或仅本机访问）
3. 客户端连接 `云服务器公网IP:9000`

### HTTPS/WSS（可选，未来）

当前客户端走 TCP 明文协议，局域网足够。公网如需加密可在后续版本加入 WSS/加密层。

## 运维与测试

### 单元测试

```bash
cd server
npm test        # node --test，21 个用例
```

### 快速连通性检查

用 PowerShell 测试 TCP 端口：

```powershell
Test-NetConnection 127.0.0.1 -Port 9000
```

### 观察日志

服务器启动后持续输出连接/房间事件：

```
conn_open  /  hello         客户端连入
room_created / room_joined  房间生命周期
conn_timeout / conn_close   超时/断开
room_destroyed              空房回收
```

通过这些日志可判断：客户端是否连上、房间是否创建、超时踢出是否正常。

## 常见问题排查

| 现象 | 排查 |
|---|---|
| 客户端提示"连接失败" | 服务器是否启动？端口 9000 是否放行？IP 是否正确？ |
| 客户端反复重连 | 看服务器是否出现 `conn_timeout`（客户端心跳 10s，服务器 30s 超时，不应误踢） |
| 本设备第二个进程报 `device_busy` | `maxConnsPerDevice=1`，一个设备只能有一个联机进程 |
| 加入房间报"角色不属于本连接" | 通常是时序问题，重新加入即可 |
| 端口被占用 `EADDRINUSE` | 结束旧 node 进程后重启 |
| 房间号/邀请码长度异常 | 检查 `roomIdLength`/`joinCodeLength` 配置并重启 |

## 房间系统

- 房间类型：**私密房**（默认，不进列表，凭房间号 + 密码加入，密码可留空=凭房间号直入）与**公共房**（出现在房间列表，可被直接加入，可选密码）
- 创建时可指定：房间名、是否公共、密码（`create_room` 的 `public`/`password` 字段）
- 公共房数量受 `room.maxPublicRooms` 限制（默认 8）
- 房主 `ownerToken` 管理（kick/settings/disband）；每设备限 1 连接（`deviceId`）；自动管理 tick（连接超时 30s / 空房 5min 回收 / 房主转移）；事件限流（emoticon 5/s、skin 2/s、mouse 20/s）

## 协议摘要

见 `../MULTIPLAYER_PLAN.md` §4（与 Qt 客户端 `src/net/protocol.h` 语义一致）：

- C→S：`hello` `create_room` `join_room` `leave_room` `list_rooms` `room_settings` `kick_member` `disband_room` `add_role` `remove_role` `skin_update` `mouse` `emoticon` `chat` `ping`
- S→C：`welcome` `room_created` `room_joined` `room_left` `room_list` `room_settings_updated` `room_closed` `peer_joined` `peer_left` `peer_kicked` `peer_skin` `peer_mouse` `peer_emoticon` `peer_chat` `pong` `error`


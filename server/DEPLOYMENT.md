# Floatee 联机服务器部署指南

Floatee 的多人联机需要一个**中转服务器**（房间管理 + 消息转发）。本文档说明如何部署、配置与运维该服务器。

> 服务器源码位于项目 `server/` 目录（Node.js + `ws`），客户端通过 **TCP 协议**连接。

---

## 1. 环境要求

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

---

## 2. 安装与依赖

```bash
cd server
npm install        # 安装唯一依赖 ws
```

依赖极少（仅 `ws`），无需编译。

---

## 3. 端口一览

| 端口 | 协议 | 用途 |
|---|---|---|
| **8764** | TCP（行协议 JSON） | **客户端主连接**（当前 Floatee 客户端用 `QTcpSocket`） |
| 8765 | WebSocket | 备用/未来客户端（Qt QWebSocket） |
| 8766 | HTTP | Admin 管理接口（需 `adminKey` 鉴权，未配置则禁用） |

> 客户端默认连接 `8764`；`8765`（WS）与客户端当前协议并行监听，端口已在服务器日志中确认。

---

## 4. 启动服务器

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
floatee_server_started { wsPort: 8765, tcpPort: 8764, adminPort: 8766, ... }
tcp_listening { host: '0.0.0.0', port: 8764 }
```

---

## 5. 配置

配置优先级：**默认值 < `server/config.json` < 环境变量**。

### 5.1 配置文件 `server/config.json`

```jsonc
{
  "server": {
    "host": "0.0.0.0",        // 监听地址：0.0.0.0=所有网卡（局域网/公网）
    "wsPort": 8765,           // WebSocket 端口
    "tcpPort": 8764,          // TCP 客户端端口
    "adminPort": 8766,        // Admin HTTP 端口
    "maxConnections": 1000    // 最大连接数
  },
  "room": {
    "capacity": 8,            // 每房间最大成员
    "maxRooms": 100,          // 最大房间数
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

### 5.2 环境变量（覆盖 config.json）

| 变量 | 作用 |
|---|---|
| `PORT` | 覆盖 WebSocket 端口（8765） |
| `TCP_PORT` | 覆盖 TCP 端口（8764） |
| `ADMIN_PORT` | 覆盖 Admin 端口（8766） |
| `ADMIN_KEY` | 覆盖 Admin 鉴权密钥 |

```bash
PORT=9000 TCP_PORT=9001 ADMIN_KEY=secret node src/index.js
```

### 5.3 修改后生效

修改 `config.json` 后需**重启服务器**：

```bash
# 先停旧进程
# Linux: 找到进程 kill，或用 pkill -f "src/index.js"
# Windows: 任务管理器结束 node.exe，或 taskkill /IM node.exe /F
# 再启动
npm start
```

> 注意：Windows 上端口被占用会报 `EADDRINUSE`，先结束旧 node 进程再启动。

---

## 6. 部署场景

### 6.1 局域网部署（最简单）

服务器与客户端在同一局域网（如家用 WiFi）：

1. 任一台机器运行服务器（`0.0.0.0` 监听）
2. **Windows 防火墙放行 TCP 8764**（以及 8765，如备用）：
   ```powershell
   # 管理员 PowerShell
   netsh advfirewall firewall add rule name="Floatee TCP 8764" dir=in action=allow protocol=TCP localport=8764
   netsh advfirewall firewall add rule name="Floatee TCP 8765" dir=in action=allow protocol=TCP localport=8765
   ```
3. 客户端在"Multiplayer → Connect..."中输入 `服务器IP:8764`（如 `192.168.10.33:8764`）

### 6.2 公网部署（云服务器）

1. 云服务器安全组/防火墙放行 **8764**（TCP）、**8765**（TCP）、**8766**（TCP，可选）
2. 建议 `maxConnsPerDevice` 保持 1，`adminKey` 设置强密码并开启 `allowRemote`（或仅本机访问）
3. 客户端连接 `云服务器公网IP:8764`

### 6.3 HTTPS/WSS（可选，未来）

当前客户端走 TCP 明文协议，局域网足够。公网如需加密可在后续版本加入 WSS/加密层（见主计划书 M5 待定项）。

---

## 7. 运维与测试

### 7.1 运行单元测试

```bash
cd server
npm test        # node --test，19 个用例
```

### 7.2 快速连通性检查

用 PowerShell 测试 TCP 端口：

```powershell
Test-NetConnection 127.0.0.1 -Port 8764
```

### 7.3 观察日志

服务器启动后持续输出连接/房间事件：

```
conn_open  /  hello         客户端连入
room_created / room_joined  房间生命周期
conn_timeout / conn_close   超时/断开
room_destroyed              空房回收
```

通过这些日志可判断：客户端是否连上、房间是否创建、超时踢出是否正常。

---

## 8. 常见问题排查

| 现象 | 排查 |
|---|---|
| 客户端提示"连接失败" | 服务器是否启动？端口 8764 是否放行？IP 是否正确？ |
| 客户端反复重连 | 看服务器是否出现 `conn_timeout`（客户端心跳 10s，服务器 30s 超时，不应误踢） |
| 本设备第二个进程报 `device_busy` | `maxConnsPerDevice=1`，一个设备只能有一个联机进程 |
| 加入房间报"角色不属于本连接" | 通常是时序问题，重新加入即可 |
| 端口被占用 `EADDRINUSE` | 结束旧 node 进程后重启 |
| 房间号/邀请码长度异常 | 检查 `roomIdLength`/`joinCodeLength` 配置并重启 |

---

## 9. 服务器文件结构

```
server/
├── config.json        # 运行时配置（覆盖默认）
├── package.json       # 依赖与脚本
├── src/
│   ├── index.js       # 入口（装配 ws + tcp + admin）
│   ├── config.js      # 默认配置 + config.json/env 覆盖
│   ├── protocol.js    # 消息类型与校验
│   ├── session.js     # 连接会话/握手/限流
│   ├── rooms.js       # 房间逻辑 + 自动管理 tick
│   ├── store.js       # 数据模型（连接/会话/房间）
│   └── admin.js       # HTTP admin 接口（可选）
└── test/              # node:test 单元测试
```

---

*最后更新：2026-08-10（M4 阶段，客户端始终全屏画布）*

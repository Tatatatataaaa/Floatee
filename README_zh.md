# Floatee

<p align="center">
  一个浮动在屏幕上的桌面宠物，支持实时联机。
</p>

> 📄 English version: <a href="README.md">English</a>

## 📝 项目简介

Floatee 是一个桌面伴侣，在桌面之上渲染一个 Tee（来自
DDraceNetwork / Teeworlds 世界）：
- 半透明、置顶的宠物，眼睛跟随光标移动
- 丰富交互：**表情圆盘**、聊天气泡、走路动画、任意 Tee 拖拽/缩放
- **实时联机**：运行中转服务器（或使用公共服务器），创建房间后即可看到
  朋友的 Tee —— 同步眼睛、皮肤、表情、HSL 配色与聊天

渲染管线移植自 DDNet 的 `tee_render`，表情圆盘交互参考 QMClient 的 `CEmoticon`。

## ✨ 功能

### 单机
- 浮动半透明宠物（全屏画布，不遮挡桌面操作）
- 光标驱动的眼睛跟随（距离相关滑动）
- 表情圆盘（16 表情 + 6 眼睛），带弹出 / 收回动画
- 可自定义**皮肤**与**表情素材**（4×4 图集 PNG）
- 每皮肤 HSL 调色、缩放（50–200%）、羽化
- 拖拽走路动画
- 极简现代 UI（半透明圆角菜单 / 弹窗 / 输入框）

### 联机
- 房间系统（6 位数字房间号，可选密码；**公共房**出现在共享房间列表，可直接加入）
- 互相显示对方的 Tee（本地可拖拽 / 缩放）
- 同步眼睛（视线方向）、皮肤（缺失自动回落 default）、HSL 配色、
  表情（实时广播）与聊天气泡
- 远端 Tee 右键菜单：隐藏 / 重置 / 踢出（仅房主）
- 从 DDNet 皮肤库自动下载远端皮肤（计划中）
- Node.js 中转服务器：房间管理 / 限流 / admin API

### 设置与 UI
- **设置窗口**：5 页布局（常规、外观、联机、休眠、实例）
  - 深浅主题切换（跟随系统或手动）
  - 缩放比例、服务器状态、使用时长实时同步
  - 房间信息显示（创建/加入/离开/列表按钮）
- **主题系统**：Fluent 风格设计，半透明玻璃质感
- **使用时长统计**：累计使用时间，支持休息提醒
- **重置位置**：多屏幕支持，找回丢失的宠物

## ❤️ 贡献者

感谢所有为本项目提交代码、反馈问题与提出改进的贡献者。

## 🚀 构建

### 依赖
- CMake ≥ 3.16
- Qt 6.5+（Core、Gui、Widgets、Network）
- C++17 编译器（MSVC 2019/2022、MinGW、GCC、Clang）

### Windows
```bat
cmake -S . -B build
cmake --build build --config Release
:: 输出：build\Release\Floatee.exe
```

### macOS / Linux
```sh
cmake -S . -B build
cmake --build build -j
```

### macOS Release 构建
```sh
mkdir build_release && cd build_release
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build . -j 8
macdeployqt Floatee.app -qmldir=. -verbose=1
codesign --force --deep --sign - Floatee.app
```
输出：`build_release/Floatee.app`（可直接分发）

### 运行联机服务器
```sh
cd server
npm install
npm start        # TCP 9000 / Admin 9001
```
部署详见 [server/README.md](server/README.md)。

## ✅ 测试

### 服务器
```sh
cd server
npm test         # node --test（21 个用例）
```

### 客户端
构建并启动 `Floatee`，右键托盘图标探索菜单。

## 🎮 使用方法

### 基础交互

| 操作 | 方法 | 说明 |
|------|------|------|
| **眼睛跟随** | 移动光标靠近 Tee | 眼睛追踪光标，越近越柔和 |
| **拖拽 Tee** | 左键拖拽 | 拖拽时 Tee 走路（动画行走循环） |
| **缩放** | 滚轮悬停在 Tee 上 | 向上放大，向下缩小（50%–200%） |
| **抚摸 Tee** | 光标悬停在 Tee 上 | 触发 ❤️ 爱心表情出现在 Tee 头顶 |

### 表情圆盘

- **打开**：右键点击 Tee
- **选择表情**：悬停外环（16 个表情），左键确认
- **切换眼睛**：悬停内环（6 种眼睛样式：Normal、Happy、Angry、Pain、Surprise、Blink），左键确认
- **关闭**：点击圆盘外区域，或按 `Esc`；圆盘带弹出/收回动画

### 聊天

- **打开聊天框**：按 `Enter`（或托盘菜单 → Send Message）
- **发送消息**：输入文字，再次按 `Enter` 发送
- **取消**：按 `Esc` 丢弃并关闭
- **自动关闭**：空白输入框 5 秒无输入自动关闭
- **输入法支持**：完全支持系统输入法（中文、日文等）

### 自动表情

Tee 会定期在头顶自动显示随机表情——无需任何操作：

- **定时触发**：每 10 秒，约 50% 概率显示随机表情
- **拖拽触发**：开始拖拽 Tee 时触发
- **设置触发**：切换眼睛或缩放级别时触发
- **抚摸触发**：光标悬停在 Tee 上触发 ❤️ 爱心表情（每次进入触发一次）
- **休眠模式**：Tee 休眠时暂停自动表情

### 联机

1. **连接服务器**：托盘 → Online → Connect（输入 `host:port`）
2. **创建房间**：托盘 → Online → Create Room（设置房间名、公开/私密、密码）
3. **加入房间**：托盘 → Online → Join Room（输入房间号），或在房间列表双击公共房间
4. **查看密码**：托盘 → Online → Show Password（显示房间号 + 密码，用于分享）
5. **离开房间**：托盘 → Online → Leave Room

加入房间后，其他玩家的 Tee 会出现在你旁边，同步显示眼睛、皮肤、表情、HSL 配色和聊天气泡。你可以拖拽/缩放其他玩家的 Tee（仅本地显示）。

### 休眠与休息

- **自动休眠**：无操作 60 秒后（可配置），Tee 进入休眠（zzz 动画、闭眼）
- **唤醒**：任何鼠标/键盘活动唤醒 Tee
- **休息提醒**：连续使用 20 分钟后（可配置），弹出提醒气泡
- **使用时长**：在 Sleep & Break 菜单或设置 → 休眠页查看累计使用时间

### 设置窗口

通过托盘菜单 → Settings 打开。五页布局：

1. **常规**：主题切换（跟随系统 / 浅色 / 深色）、总在最前、颜色调整
2. **外观**：皮肤、眼睛、缩放、羽化强度、表情素材
3. **联机**：服务器地址、连接/断开、房间信息（创建/加入/离开/列表）
4. **休眠**：休眠超时、休息提醒间隔、长休眠重置阈值
5. **实例**：多实例管理（启动新实例、自定义配置、配置目录）

### 多屏幕

如果 Tee 在断开显示器后消失：
- 使用托盘菜单 → **Reset Position** 将 Tee 放回最近的可见屏幕

## 🖼 截图

<p align="center">
  <img src="assets/readme/Desktop&Tee.png" width="45%" alt="桌面 & Tee" />
  <img src="assets/readme/Desktop&Tees.png" width="45%" alt="桌面 & 多个 Tee（联机）" />
</p>

<p align="center">
  <img src="assets/readme/emotion.png" width="30%" alt="表情圆盘" />
  <img src="assets/readme/Online.png" width="30%" alt="联机页面" />
  <img src="assets/readme/OnlineEmotion.png" width="30%" alt="联机 + 表情" />
</p>

<p align="center">
  <img src="assets/readme/SettingNormal.png" width="45%" alt="设置 - 常规页" />
  <img src="assets/readme/SettingAppearance.png" width="45%" alt="设置 - 外观页" />
</p>

1. **主界面**：Tee 浮动在桌面上，眼睛跟随光标移动
2. **多人模式**：多个 Tee 在同一房间（同步眼睛/皮肤/聊天）
3. **表情圆盘**：16 个表情 + 6 种眼睛，带弹出动画
4. **联机页面**：服务器连接、房间信息、房间操作按钮
5. **联机 + 表情**：多人模式下的表情圆盘
6. **设置 - 常规页**：主题切换、窗口设置
7. **设置 - 外观页**：皮肤选择、眼睛/缩放/羽化控件

## 🏛 致谢

- **DDNet / Teeworlds** —— Tee 渲染管线（`tee_render`）与皮肤数据库来自
  DDraceNetwork 项目
- **QMClient** —— 表情圆盘交互参考
- **Qt Project** —— Qt 6 框架（LGPL）
- **ElaWidgetTools** —— UI 设计参考（Fluent 风格）

## 📜 许可

本项目基于 / 参考 DDNet 与 QMClient。由于渲染管线与交互代码衍生自 GPL-3.0
项目，本项目以 **GNU General Public License v3.0** 发布。

Qt 以动态链接方式按 **LGPL-3.0** 使用。

详见根目录 [LICENSE](LICENSE) 与 [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt)。

## 📮 备注

本项目是个人化的桌面宠物，**不是** DDNet 或 QMClient 的官方产品。皮肤素材
版权归各自作者所有，请遵循各素材的许可（如 DDNet 皮肤库的 CC BY-SA）。

### AI Vibe Coding

本项目采用 **AI Vibe Coding** 工作流开发——AI 代理（由 MiMo 驱动的 GitHub
Copilot）与开发者在 IDE 中实时协作。AI 代理负责代码生成、Bug 诊断、多步
重构、构建验证和文档编写，开发者主导设计决策并提供视觉反馈。

工作流要点：
- **迭代精炼**：功能通过开发者与 AI 代理之间的紧密反馈循环构建、测试和改进
- **跨平台调试**：AI 代理通过运行时日志、崩溃报告和像素分析诊断平台特定问题
（Windows/macOS/Linux）
- **架构指导**：AI 代理在编码前阅读参考项目（如 ElaWidgetTools）并提出实现方案
- **人工审核**：所有更改在提交前均由开发者审查和批准

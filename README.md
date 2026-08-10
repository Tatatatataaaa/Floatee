# Floatee

<p align="center">
  A floating desktop pet for your screen, with real-time multiplayer support.
</p>

> 📄 中文说明见下文 [中文](#中文)

## 📝 Project Overview

Floatee is a desktop companion that renders a Tee (a character from the
DDraceNetwork / Teeworlds universe) floating above your desktop. It supports:

- A translucent, always-on-top pet that follows your cursor with its eyes.
- Rich interaction: an **emoticon wheel**, chat bubbles, walking animation,
  per-Tee dragging and zooming.
- **Real-time multiplayer**: run a relay server (or use a public one), create a
  room, and see your friends' Tees appear next to yours — with synced eyes,
  skins, emotes, HSL colours and chat.

The rendering pipeline is a faithful port of DDNet's `tee_render`, and the
emoticon-wheel interaction is inspired by QMClient's `CEmoticon`.

## ✨ Features

### Standalone
- Floating translucent pet (full-screen canvas, non-intrusive)
- Cursor-driven eye follow with distance-based travel
- Emoticon wheel (16 emotes + 6 eyes) with pop-in / retract animations
- Customisable **skins** and **emoticon sets** (4×4 atlas PNGs)
- Per-skin HSL colour adjustment, zoom (50–200%), feathering
- Walking animation while dragging
- Minimal modern UI (translucent rounded menus, dialogs, inputs)

### Multiplayer
- Room system (6-digit room ID + 4-char invite code)
- See other players' Tees, drag/zoom them locally
- Synced eyes (look direction), skins (auto-fallback to default),
  HSL colours, emotes (real-time broadcast) and chat bubbles
- Remote-Tee context menu: hide / reset / kick (owner-only)
- Automatic download of remote skins from the DDNet skin database (planned)
- Node.js relay server with room management, throttling and admin API

## ❤️ Contributors

We would like to thank all contributors who have submitted code, reported
issues and suggested improvements for this project.

## 🚀 Build

### Requirements
- CMake ≥ 3.16
- Qt 6.5+ (Core, Gui, Widgets, Network)
- A C++17 compiler (MSVC 2019/2022, MinGW, GCC, Clang)

### Windows
```bat
cmake -S . -B build
cmake --build build --config Release
:: output: build\Release\Floatee.exe
```

### macOS / Linux
```sh
cmake -S . -B build
cmake --build build -j
```

### Run the multiplayer server
```sh
cd server
npm install
npm start        # TCP 8764 / WS 9001 / Admin 8766
```
See [server/DEPLOYMENT.md](server/DEPLOYMENT.md) for deployment details.

## ✅ Test

### Server
```sh
cd server
npm test         # node --test (19 cases)
```

### Client
Build and launch `Floatee`; right-click the tray icon to explore menus.

## 🏛 Credits

- **DDNet / Teeworlds** — the Tee rendering pipeline (`tee_render`) and skin
  database are from the DDraceNetwork project
- **QMClient** — emoticon wheel interaction reference
- **Qt Project** — Qt 6 framework (LGPL)

## 📜 License

This project is based on / inspired by DDNet and QMClient. Because the
rendering pipeline and interaction code are derived from GPL-3.0 projects, this
project is released under the **GNU General Public License v3.0**.

Qt is used as a dynamically-linked library under the **LGPL-3.0**.

See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt)
for details.

## 📮 Notes

This project is a personalised desktop pet and is **not** an official product
of DDNet or QMClient. Skin artwork belongs to its respective authors; please
respect each skin's license (e.g. CC BY-SA on DDNet's skin database).

---

## 中文

### 📝 项目简介

Floatee 是一个浮动桌面宠物，在桌面之上渲染一个 Tee（来自
DDraceNetwork / Teeworlds 世界）：
- 半透明、置顶的宠物，眼睛跟随光标移动
- 丰富交互：**表情圆盘**、聊天气泡、走路动画、任意 Tee 拖拽/缩放
- **实时联机**：运行中转服务器（或使用公共服务器），创建房间后即可看到
  朋友的 Tee —— 同步眼睛、皮肤、表情、HSL 配色与聊天

渲染管线移植自 DDNet 的 `tee_render`，表情圆盘交互参考 QMClient 的 `CEmoticon`。

### ✨ 功能

**单机**：浮动半透明宠物、眼睛跟随、表情圆盘（16 表情 + 6 眼睛，含弹出/
收回动画）、自定义皮肤与表情素材（4×4 图集 PNG）、每皮肤 HSL 调色、缩放
（50–200%）、羽化、拖拽走路动画、极简现代 UI（半透明圆角菜单/弹窗/输入框）。

**联机**：房间系统（6 位数字房间号 + 4 位邀请码）、互相显示 Tee（本地可
拖动/缩放）、同步眼睛/皮肤（缺失回落 default）/HSL/表情（实时广播）/聊天、
远端 Tee 右键菜单（隐藏/重置/踢出[仅房主]）、从 DDNet 皮肤库自动下载远端
皮肤（计划中）、Node.js 中转服务器（房间管理/限流/admin API）。

### 🚀 构建

- 依赖：CMake ≥ 3.16、Qt 6.5+（Core/Gui/Widgets/Network）、C++17 编译器
- Windows：`cmake -S . -B build` → `cmake --build build --config Release`
- 服务器：`cd server && npm install && npm start`（部署详见 `server/DEPLOYMENT.md`）

### 📜 许可

本项目基于/参考 DDNet 与 QMClient。由于渲染管线与交互代码衍生自 GPL-3.0
项目，本项目以 **GNU General Public License v3.0** 发布；Qt 以 **LGPL-3.0**
动态链接方式使用。详见根目录 `LICENSE` 与 `THIRD_PARTY_NOTICES.txt`。

# Floatee

<p align="center">
  A floating desktop pet for your screen, with real-time multiplayer support.
</p>

> 📄 This document is available in <a href="README_zh.md">中文</a>

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

*中文文档见 [README_zh.md](README_zh.md)。*

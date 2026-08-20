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

### Settings & UI

- **Settings window**: 5-page layout (General, Appearance, Network, Sleep, Instance)
  - Dark/light theme toggle (follows system or manual)
  - Real-time sync of zoom scale, server status, usage time
  - Room info display with create/join/leave/list buttons
- **Theme system**: Fluent design with translucent glass style
- **Usage tracking**: cumulative usage time with break reminders
- **Reset Position**: multi-screen support for lost pet recovery

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

### macOS Release Build

```sh
mkdir build_release && cd build_release
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build . -j 8
macdeployqt Floatee.app -qmldir=. -verbose=1
codesign --force --deep --sign - Floatee.app
```

Output: `build_release/Floatee.app` (ready for distribution)

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

## 🎮 Usage

### Basic Interaction

| Action | How | Description |
|--------|-----|-------------|
| **Eye follow** | Move cursor near the Tee | Eyes track your cursor; the closer, the gentler the movement |
| **Drag Tee** | Left-click + drag | Tee walks while dragging (animated walk cycle) |
| **Zoom** | Mouse wheel over Tee | Scroll up to zoom in, down to zoom out (50%–200%) |
| **Tee petting** | Hover cursor over Tee | Triggers a ❤️ hearts emoticon above the Tee |

### Emoticon Wheel

- **Open**: Right-click the Tee
- **Select emote**: Hover over the outer ring (16 emotes), left-click to confirm
- **Select eyes**: Hover over the inner ring (6 eye styles: Normal, Happy, Angry, Pain, Surprise, Blink), left-click to confirm
- **Close**: Click outside the wheel, or press `Esc`; the wheel animates in/out smoothly

### Chat

- **Open chat**: Press `Enter` (or use tray menu → Send Message)
- **Send message**: Type text, press `Enter` again to send
- **Cancel**: Press `Esc` to discard and close
- **Auto-close**: Empty input box closes automatically after 5 seconds of inactivity
- **IME support**: System input method (Chinese, Japanese, etc.) is fully supported

### Auto Emoticons

The Tee periodically shows random emoticons above its head — no action needed:

- **Random trigger**: Every 10 seconds, a ~50% chance to display a random emoticon
- **Drag trigger**: Starting to drag the Tee triggers an emoticon
- **Settings trigger**: Switching eyes or zoom level triggers an emoticon
- **Petting trigger**: Hovering cursor over the Tee triggers a ❤️ hearts emoticon (once per entry)
- **Sleep mode**: Auto emoticons are paused while the Tee is sleeping

### Multiplayer

1. **Connect**: Tray → Online → Connect (enter `host:port`)
2. **Create room**: Tray → Online → Create Room (set name, public/private, password)
3. **Join room**: Tray → Online → Join Room (enter room ID) or double-click a public room in the room list
4. **Show password**: Tray → Online → Show Password (displays room ID + password for sharing)
5. **Leave room**: Tray → Online → Leave Room

In a room, other players' Tees appear next to yours with synced eyes, skins, emotes, HSL colours and chat bubbles. You can drag/zoom other players' Tees locally.

### Sleep & Break

- **Auto sleep**: After 60 seconds of inactivity (configurable), the Tee falls asleep (zzz animation, eyes close)
- **Wake up**: Any mouse/keyboard activity wakes the Tee
- **Break reminder**: After 20 minutes of continuous use (configurable), a reminder bubble appears
- **Usage tracking**: View cumulative usage time in the Sleep & Break menu or Settings → Sleep page

### Settings Window

Open via tray menu → Settings. Five pages:

1. **General**: Theme toggle (follows system / light / dark), Always on Top, Color Adjust
2. **Appearance**: Skin, eyes, zoom, feather strength, emoticon set
3. **Network**: Server address, connect/disconnect, room info (create/join/leave/list)
4. **Sleep**: Sleep timeout, break reminder interval, reset-after-sleep threshold
5. **Instance**: Multi-instance management (launch new, custom profile, config folder)

### Multi-screen

If the Tee disappears after disconnecting a monitor:
- Use tray menu → **Reset Position** to bring it back to the nearest visible screen

## 🖼 Screenshots

<p align="center">
  <img src="assets/readme/Desktop&Tee.png" width="45%" alt="Desktop & Tee" />
  <img src="assets/readme/Desktop&Tees.png" width="45%" alt="Desktop & Tees (Multiplayer)" />
</p>

<p align="center">
  <img src="assets/readme/emotion.png" width="30%" alt="Emoticon Wheel" />
  <img src="assets/readme/Online.png" width="30%" alt="Online Page" />
  <img src="assets/readme/OnlineEmotion.png" width="30%" alt="Online with Emoticon" />
</p>

<p align="center">
  <img src="assets/readme/SettingNormal.png" width="45%" alt="Settings - General Page" />
  <img src="assets/readme/SettingAppearance.png" width="45%" alt="Settings - Appearance Page" />
</p>

1. **Main Interface**: Tee floating on desktop with cursor-driven eye follow
2. **Multiplayer**: Multiple Tees in a room (synced eyes, skins, chat)
3. **Emoticon Wheel**: 16 emotes + 6 eyes with pop-in animations
4. **Network Page**: Server connection, room info, and room operations
5. **Online with Emoticon**: Emoticon wheel in multiplayer mode
6. **Settings - General**: Theme toggle, window settings
7. **Settings - Appearance**: Skin selection, eye/size/feather controls

## 🏛 Credits

- **DDNet / Teeworlds** — the Tee rendering pipeline (`tee_render`) and skin
  database are from the DDraceNetwork project
- **QMClient** — emoticon wheel interaction reference
- **Qt Project** — Qt 6 framework (LGPL)
- **ElaWidgetTools** — UI design inspiration (Fluent style)

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

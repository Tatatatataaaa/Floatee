# Floatee 项目进度

## 项目概述

Floatee 是一个跨平台桌面宠物应用，使用 Qt6 (C++/OBJC++) 编写。一个浮动的 T 恤角色显示在桌面上，眼睛会跟随鼠标移动，支持窗口侧边隐藏、定时护眼提醒、以及系统托盘常驻。

**技术栈**: Qt 6 (Core/Gui/Widgets), CMake 3.16+, C++17, Objective-C++ (macOS)

---

## 现有功能

### 1. 浮动桌面宠物

- 96×96 无边框半透明窗口，显示 T 恤角色
- 角色皮肤从 `assets/skin/` 通过 Qt 资源系统 (qrc) 加载进内存，无运行时文件依赖
- **眼睛跟随鼠标**: 眼睛（30×27 像素）根据鼠标位置追踪移动，有范围限制（15px 半径），靠近时变为笑脸
- **右键切换表情**: 右键点击切换普通眼 / Clever 眼
- 左键拖拽移动角色位置
- 系统托盘图标常驻，双击托盘图标退出

### 2. 窗口侧边隐藏 (WindowSideHide)

- 检测鼠标释放时焦点窗口是否部分拖出屏幕边缘
- 如果窗口超过一半在屏幕外 → 自动动画滑出屏幕（左侧或右侧）
- 鼠标移动到对应屏幕边缘 → 隐藏的窗口滑回屏幕内
- 鼠标离开该窗口范围 → 窗口再次隐藏
- **Windows**: 通过 Win32 API (`GetForegroundWindow`, `QWindow::fromWinId`) 直接操作窗口句柄，动画用 `QPropertyAnimation`
- **macOS**: 通过 Accessibility API (`AXUIElement`) 获取/设置窗口位置，动画用 `QVariantAnimation`
- **Linux**: 已禁用（平台不支持此功能）

### 3. 护眼提醒 (TeEyes)

- 可配置的时间间隔（默认 600 秒 / 10 分钟）
- 到时全屏显示背景图覆盖桌面，强制休息
- 按空格键可标记当前前台窗口标题/类名为豁免（下次不触发）
- 按 Escape 跳过本次提醒
- 可在 Windows 上嵌入 Microsoft To Do 窗口
- 配置存储在 `teeyes.json`，通过 QStandardPaths 写入应用数据目录

### 4. 跨平台窗口信息 (PlatformWindowInfo)

- 抽象接口获取前台窗口标题和类名
- Windows: Win32 API (`GetForegroundWindow`, `GetWindowText`)
- macOS: Accessibility API (`AXUIElement`)
- Linux: X11 (`XGetInputFocus`, `XFetchName`)

---

## 最近完成的工作

### 2026-06-19 — macOS .app 打包 & Finder 启动修复

#### 问题
- 从 Finder 双击启动时崩溃，终端启动正常
- 根因: `setup.json` / `teeyes.json` 使用相对路径 `"./"` 解析为 CWD。Finder 启动时 CWD 为 `/`（不可写）

#### 修复
1. **配置文件路径改为绝对路径** (`8a3c2f1`)
   - `floatee.cpp`: `Path_Setup` 改用 `QStandardPaths::GenericDataLocation + "/Floatee/setup.json"`
   - `teeyes.cpp`: `Path_Data` 改用 `QStandardPaths::GenericDataLocation + "/Floatee/teeyes.json"`
   - 所有平台统一存储在 `~/Library/Application Support/Floatee/` (macOS) 或等效路径
   - `main.cpp`: 设置 `QApplication::setApplicationName("Floatee")` 确保路径一致

2. **macOS .app Bundle 打包** (`8a3c2f1`)
   - `CMakeLists.txt`: 添加 `MACOSX_BUNDLE TRUE` + bundle 属性 + `configure_file` 生成 Info.plist
   - `Info.plist.in`: 新建模板，包含 `NSHighResolutionCapable`, `NSPrincipalClass`, `NSAppleEventsUsageDescription`
   - 构建产物从裸 Mach-O 二进制变为 `Floatee.app/Contents/MacOS/Floatee`

#### 修改文件
- `src/ui/floatee.h` — Path_Setup 初始化方式变更
- `src/ui/floatee.cpp` — QStandardPaths + QDir, BodyLabel 渲染
- `src/ui/teeyes.h` — 添加 Path_Data 成员
- `src/ui/teeyes.cpp` — QStandardPaths + QDir
- `src/main.cpp` — setApplicationName
- `CMakeLists.txt` — MACOSX_BUNDLE 配置
- `Info.plist.in` — 新建

---

### 2026-06-19 — 资源依赖修复

#### 问题
- 代码依赖 `./Data/Skin.png` 和 `./Data/*.png` 运行时文件，但皮肤实际在 `assets/skin/`
- 代码在运行时用 `save()` 往硬盘写临时图片文件

#### 修复
- `Floatee.qrc`: 添加 9 个皮肤文件到 `/skins` 前缀，使用短别名
- `teedrawer.h/cpp`: 重写加载逻辑，所有 pixmap 从 qrc 加载到内存，移除所有磁盘写入
- `floatee.cpp`: 用 `QLabel::setPixmap` 替代 stylesheet background-image

#### 修改文件
- `Floatee.qrc`
- `src/core/teedrawer.h`
- `src/core/teedrawer.cpp`
- `src/ui/floatee.cpp`

---

### 2026-06-19 — macOS 构建修复

#### 问题
- `windowsidehide.cpp` 使用了 `@autoreleasepool` 等 Objective-C 语法但被当作 C++ 编译
- `AppKit` 头文件缺失导致 `NSEvent`, `NSWorkspace` 等符号未定义

#### 修复
- `CMakeLists.txt`: 在 `project()` 后添加 `enable_language(OBJCXX)`；对 `windowsidehide.cpp` 设置 `LANGUAGE OBJCXX`
- `windowsidehide.cpp`: 在 macOS 分支添加 `#import <AppKit/AppKit.h>`

#### 修改文件
- `CMakeLists.txt`
- `src/ui/windowsidehide.cpp`

---

### 2026-06-20 — 系统托盘菜单 & 始终置顶

#### 问题
- Tee 窗口置顶开关只能通过手动编辑 `setup.json` 控制，没有 UI 入口
- macOS 上 `Qt::Tool` 窗口会在应用失去焦点时自动隐藏（点击其他窗口 Tee 消失）
- `setWindowFlags()` 重建原生窗口导致置顶标志不生效

#### 修复
1. **系统托盘右键菜单** — 新建 `QMenu`，包含:
   - "Always on Top" — 可勾选菜单项，点击切换置顶状态
   - "Quit" — 退出应用
   - 菜单通过 `QSystemTrayIcon::setContextMenu()` 设置，全平台可用

2. **置顶开关逻辑** — `toggleAlwaysOnTop()` 槽函数:
   - 用 `setWindowFlag(Qt::WindowStaysOnTopHint, on)` 替代 `setWindowFlags()` — 单标志修改不重建原生窗口
   - 修改后调用 `show()` 确保生效
   - 自动保存到 `setup.json` 持久化

3. **macOS Tool 窗口自动隐藏修复**:
   - 在 `Initialize()` 添加 `setAttribute(Qt::WA_MacAlwaysShowToolWindow, true)` — 禁止 NSPanel 在应用失焦时消失
   - `teeyes.cpp` 中 `Stop()` 也改用 `setWindowFlag`

#### 修改文件
- `src/ui/floatee.h` — 添加 TrayMenu, AlwaysOnTopAction, toggleAlwaysOnTop()
- `src/ui/floatee.cpp` — 托盘菜单构建 + 切换逻辑 + WA_MacAlwaysShowToolWindow
- `src/ui/teeyes.cpp` — setWindowFlags → setWindowFlag

---

## 当前项目结构

```
Floatee/
├── CMakeLists.txt              # CMake 构建（含 macOS .app bundle）
├── Info.plist.in               # macOS Bundle 模板
├── Floatee.qrc                 # Qt 资源文件（皮肤、保护图）
├── PROGRESS.md                 # 本文件
├── assets/
│   ├── skin/                   # 角色皮肤 PNG（9 个）
│   └── bg/                     # 护眼背景图
└── src/
    ├── main.cpp
    ├── core/
    │   ├── jsonopt.h/cpp       # JSON 文件读写工具
    │   └── teedrawer.h/cpp     # 皮肤精灵切图、眼睛合成、色调变换
    ├── ui/
    │   ├── floatee.h/cpp       # 主窗口：宠物角色、拖拽、托盘菜单
    │   ├── floatee.ui          # Qt Designer 表单
    │   ├── windowsidehide.h/cpp# 窗口侧边隐藏（Win/macOS 双实现）
    │   ├── teeyes.h/cpp        # 护眼提醒：定时全屏、窗口豁免
    │   └── teeyes.ui           # Qt Designer 表单
    └── platform/
        ├── platformwindowinfo.h        # 平台抽象接口
        ├── platformwindowinfo_win.cpp  # Windows 实现
        ├── platformwindowinfo_mac.mm   # macOS 实现
        └── platformwindowinfo_x11.cpp  # Linux 实现
```

## 构建命令

```bash
cmake -S . -B build_check -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release
# 产物: build_check/Floatee.app (macOS) 或 build_check/Floatee.exe (Windows)
```

## 待解决

- [ ] macOS 窗口侧边隐藏：AX API `setAttribute` 对 Apple 原生应用/SwiftUI/Catalyst 无效，需改进（见方案1）
- [ ] `Floatee.pro` (qmake) 已过期，与 CMake 不同步，可考虑移除
- [ ] 皮肤切换机制尚未暴露 UI（当前硬编码 Tata.png）

---

*最后更新: 2026-06-20*

# Floatee 项目进度

## 项目概述

Floatee 是一个跨平台桌面宠物应用，使用 Qt6 (C++/OBJC++) 编写。一个浮动的 T 恤角色显示在桌面上，眼睛会跟随鼠标移动，支持窗口侧边隐藏、定时护眼提醒、以及系统托盘常驻。

**技术栈**: Qt 6 (Core/Gui/Widgets), CMake 3.16+, C++17, Objective-C++ (macOS)

---

## 现有功能

### 1. 浮动桌面宠物

- 96×96 无边框半透明窗口，显示 T 恤角色，始终置顶
- 角色皮肤从 `assets/skin/` 通过 Qt 资源系统 (qrc) 加载进内存，无运行时文件依赖
- 9 个内置皮肤 + 外部 `skins/` 目录动态加载，托盘菜单一键切换
- **眼睛跟随鼠标**: 眼睛由 tee_render 管线直接渲染在身体上（随光标方向在脸部内滑动），靠近时变为笑脸
- **眼睛类型**: Normal / Happy / Angry / Pain / Surprise，托盘 Eyes 子菜单切换，右键循环
- **皮肤颜色调整**: Hue / Saturation / Lightness 三轴滑动条，按皮肤持久化到 `setup.json`
- 左键拖拽移动角色位置
- 系统托盘图标常驻，右键菜单含置顶、WSH、护眼、眼睛、皮肤、颜色调整、退出

### 2. 窗口侧边隐藏 (WindowSideHide)

- 检测鼠标释放时焦点窗口是否部分拖出屏幕边缘
- 如果窗口超过一半在屏幕外 → 自动动画滑出屏幕（左侧或右侧）
- 鼠标移动到对应屏幕边缘 → 隐藏的窗口滑回屏幕内
- 鼠标离开该窗口范围 → 窗口再次隐藏
- **Windows**: 通过 Win32 API (`GetForegroundWindow`, `QWindow::fromWinId`) 直接操作窗口句柄，动画用 `QPropertyAnimation`
- **macOS**: 通过 Accessibility API (`AXUIElement`) 获取/设置窗口位置，动画用 `QVariantAnimation`
- **Linux**: 已禁用（平台不支持此功能）
- 托盘菜单可勾选启用/禁用

### 3. 护眼提醒 (TeEyes)

- 可配置的时间间隔（默认 600 秒 / 10 分钟）
- 到时全屏显示背景图覆盖桌面，强制休息
- 按空格键可标记当前前台窗口标题/类名为豁免（下次不触发）
- 按 Escape 跳过本次提醒
- 可在 Windows 上嵌入 Microsoft To Do 窗口
- 配置存储在 `teeyes.json`，通过 `QStandardPaths::GenericDataLocation` 写入应用数据目录
- 托盘菜单可勾选启用/禁用

### 4. 跨平台窗口信息 (PlatformWindowInfo)

- 抽象接口获取前台窗口标题和类名
- Windows: Win32 API (`GetForegroundWindow`, `GetWindowText`)
- macOS: Accessibility API (`AXUIElement`)
- Linux: X11 (`XGetInputFocus`, `XFetchName`)

---

## Git 分支状态

- `main`（当前 HEAD `30cb0b4`）：桌面端最新代码
- `android`（`e9acae6`）：基于 main 的 Android 悬浮窗移植分支

---

## 最近完成的工作

### 2026-06-30 — 初始提交（`929df33`）

- 项目初始版本，已包含：
  - Floatee 主窗口、托盘、置顶
  - 眼睛跟随鼠标、右键 Pain 切换
  - WindowSideHide（Win/macOS）
  - TeEyes 护眼提醒
  - PlatformWindowInfo 抽象层（Win/macOS/Linux）
  - CMake + macOS bundle + `Info.plist.in`
  - `setup.json` / `teeyes.json` 初始即使用 `QStandardPaths::GenericDataLocation`

---

### 2026-06-30 — 皮肤切分标准化修正（`296d54c`）

#### 问题
- 皮肤图片切分尺寸不合规（如眼睛应为 32×32，实际 ~17×27）
- 两个眼睛是相同图案复制，未做镜像
- 元素布局随手动测量，不符合标准切分大小

#### 修复
1. **标准化源图切分坐标** — 从 4K 模板等比例缩放到 256×128：
   - 身体: (0, 0, 96, 96) = 1536×1536 / 16
   - 普通眼: (64, 96, 32, 32) = G1 区 / 16
   - 生气眼: (96, 96, 32, 32) = G2 区 / 16
   - 笨拙眼: (128, 96, 32, 32) = G3 区 / 16
   - 快乐眼: (160, 96, 32, 32) = G4 区 / 16
   - 脚掌: (192, 32, 64, 32) = E 区 / 16
   - 所有坐标通过比例因子 `sx/sy` 自动适配 512×256 等 2x 皮肤

2. **右眼镜像** — 用 `QTransform::fromScale(-1, 1)` 水平翻转右眼，形成自然的左右对称

3. **显示尺寸调整** — 眼部显示 32×32，眼对画布 52×32，脚掌 64×32

#### 修改文件
- `src/core/teedrawer.cpp` — 重写切图逻辑

---

### 2026-06-30 ~ 2026-07-01 — 眼睛 / 脚掌显示微调

经过多轮提交调整，最终稳定为：
- 眼睛 QLabel 尺寸 52×32，位置 (24, 28)
- 眼睛画布 52×32，两眼间距 16px
- 托盘图标眼睛位置 (30, 28)
- 脚掌源 `(192, 32, 64, 32)`，显示 64×32，右脚镜像
- 图层顺序：左脚 (0, 56) → 右脚 (34, 56) → 身体 (0, 0)

---

### 2026-07-01 — 皮肤切换功能（`638623a`）

#### 新增
- **托盘菜单 Skin 子菜单** — 列出全部 9 个内置皮肤，QActionGroup 互斥选中，当前皮肤打勾
- 切换时自动更新 BodyLabel、TeeEyes、托盘图标、窗口图标
- 选择持久化到 `setup.json` 的 `Skin` 字段，启动时自动加载
- **切换剪影修复** — 切换前 `hide()` 窗口，更新完 `show()`，强制 macOS 丢弃旧 backing store

#### 修改文件
- `src/ui/floatee.h` — 添加 SkinMenu, SkinGroup, CurrentSkin, switchSkin()
- `src/ui/floatee.cpp` — 皮肤子菜单构建 + 切换逻辑 + hide/show 修复

---

### 2026-07-01 — 托盘菜单扩展

#### 提交 `b427075` — 新增 WSH / Eye Care 开关
- 托盘菜单新增 "Window Side Hide" 和 "Eye Care" 可勾选菜单项
- 点击切换对应模块启用状态，并持久化到 `setup.json`

#### 提交 `88fa885` / `0df52b4` / `d01e28f` / `3e69689` — 眼睛子菜单
- 新增 "Eyes" 子菜单：Normal / Happy / Angry / Pain / Surprise
- 右键点击循环切换眼睛类型（最终逻辑：循环全部 5 种）
- 当前眼睛类型持久化到 `setup.json` 的 `Eye` 字段
- 移除鼠标靠近时的 Happy 自动覆盖，避免与右键循环冲突

---

### 2026-07-01 — Windows 构建优化（`fe7db4b`）

- `CMakeLists.txt`: 设置 `WIN32_EXECUTABLE TRUE`，Windows 启动时不显示控制台窗口
- MinGW 构建后自动复制 `libstdc++-6.dll`、`libgcc_s_seh-1.dll`、`libwinpthread-1.dll`
- 镜像方式统一为 `QTransform::fromScale(-1, 1)` 以兼容 Qt 6.7.2

---

### 2026-07-01 — HSL 颜色调整、外部皮肤、Windows 子系统（`30cb0b4`）

#### 新增
- **Color Adjust 对话框**: Hue (-180~180)、Saturation (0~200%)、Lightness (0~200%)
  - 实时预览，OK 后按皮肤保存到 `setup.json` 的 `SkinHSL` 字段
  - Cancel 时回退到原始 HSL
- **外部皮肤**: 启动时扫描应用目录下 `skins/` 文件夹中的 `*.png`，动态加入 Skin 子菜单
- **Open Skins Folder** 菜单项：一键打开外部皮肤目录

#### 修改文件
- `src/ui/floatee.h/.cpp` — `openColorDialog()`、外部皮肤扫描
- `src/core/teedrawer.h/.cpp` — `adjustHsl()`、`load(skin, hue, sat, light)`
- `CMakeLists.txt` — `WIN32_EXECUTABLE`、MinGW DLL 复制、`Floatee.icns` bundle
- `Info.plist.in` — 添加 `CFBundleIconFile`

#### 注意
- 本次提交把 `setup.json` 路径从 `QStandardPaths::GenericDataLocation + "/Floatee"` **改回了 `QCoreApplication::applicationDirPath()`**，以便与外部皮肤目录放在同一位置。
- `teeyes.json` 仍保留在 `QStandardPaths::GenericDataLocation + "/Floatee"`。

---

### 2026-07-02 — Android 移植分支（`e9acae6`）

- 新增 `android` 分支
- 使用 `SYSTEM_ALERT_WINDOW` 实现悬浮窗
- 触摸拖拽 + 多触点的眼睛循环
- 硬编码默认 Tata 皮肤，无皮肤切换
- 禁用 TeEyes 和 WindowSideHide（桌面端功能）
- 新增 Android 平台 stub `platformwindowinfo_android.cpp`

---

## 当前项目结构

```
Floatee/
├── CMakeLists.txt              # CMake 构建（含 macOS .app bundle / Android）
├── Info.plist.in               # macOS Bundle 模板
├── Floatee.qrc                 # Qt 资源文件（皮肤、保护图、主资源）
├── Floatee.pro                 # qmake 配置（已过期，未维护）
├── PROGRESS.md                 # 本文件
├── assets/
│   ├── skin/                   # 内置角色皮肤 PNG（9 个）
│   ├── bg/                     # 护眼背景图
│   ├── main/                   # 历史遗留资源（可能未使用）
│   └── Floatee.icns            # macOS app 图标
├── android/                    # Android 移植（android 分支）
├── tee_render/                 # DDNet 渲染管线移植（ITeeRenderBackend 抽象，C++17）
└── src/
    ├── main.cpp
    ├── core/
    │   ├── jsonopt.h/cpp       # JSON 文件读写工具
    │   ├── teedrawer.h/cpp     # tee_render 渲染驱动：身体/脚/眼睛合成、HSL 变换
    │   └── tee_qt_backend.h/cpp# tee_render 的 Qt 软件光栅后端（QPainter 绘制 quad）
    ├── ui/
    │   ├── floatee.h/cpp       # 主窗口：宠物角色、拖拽、托盘菜单、颜色调整
    │   ├── floatee.ui          # Qt Designer 表单
    │   ├── windowsidehide.h/cpp# 窗口侧边隐藏（Win/macOS 双实现）
    │   ├── teeyes.h/cpp        # 护眼提醒：定时全屏、窗口豁免
    │   └── teeyes.ui           # Qt Designer 表单
    └── platform/
        ├── platformwindowinfo.h                # 平台抽象接口
        ├── platformwindowinfo_win.cpp          # Windows 实现
        ├── platformwindowinfo_mac.mm           # macOS 实现
        ├── platformwindowinfo_x11.cpp          # Linux 实现
        └── platformwindowinfo_android.cpp      # Android stub（android 分支）
```

## 构建命令

```bash
# 桌面端
cmake -S . -B build_check -DCMAKE_BUILD_TYPE=Release
cmake --build build_check --config Release
# 产物: build_check/Floatee.app (macOS) 或 build_check/Floatee.exe (Windows)

# Android（需提前配置 Qt Android 工具链和 NDK）
cmake -S . -B build_android -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26
cmake --build build_android
```

---

## 本次会话修复（2026-08-09）

- [x] **统一配置文件路径到 `AppDataLocation`**
  - `setup.json`、`teeyes.json`、外部皮肤 `skins/` 目录统一放到 `QStandardPaths::AppDataLocation`
  - 修改文件：`src/ui/floatee.cpp`、`src/ui/teeyes.cpp`

- [x] **修复 `TeEyes` 定时器泄漏**
  - 切换 Interval / Duration 定时器前先 `killTimer(Id)`
  - `Stop()` 统一管理定时器重启，`keyPressEvent` 不再重复启动
  - 修改文件：`src/ui/teeyes.cpp`

- [x] **Windows `findAndEmbedWindow` Unicode 修复**
  - `FindWindowA` + `reinterpret_cast<LPCSTR>` 改为 `FindWindowW` + `std::wstring`
  - 避免非 ASCII 窗口标题/类名匹配失败
  - 修改文件：`src/platform/platformwindowinfo_win.cpp`

- [x] **皮肤渲染替换为 tee_render 管线**
  - 引入 `tee_render/`（DDNet/QmClient 渲染管线 C++17 移植，`ITeeRenderBackend` 抽象）
  - 新增 `src/core/tee_qt_backend.h/.cpp`：Qt 软件光栅后端，用 `QPainter` 把 quad 绘制到 QPixmap
  - 重写 `src/core/teedrawer.cpp`：内部使用 `CTeeRenderer`，`configureRegions()` 按 256×128 参考系配置身体/轮廓/脚/眼睛精灵区域，`TeeBare/Tee/TeeEyes...` 等对外 QPixmap 接口不变
  - `CMakeLists.txt`：`add_subdirectory(tee_render)` + 链接 `tee_render`，MSVC 加 `/Zc:__cplusplus` 与 `NOMINMAX`
  - 眼睛/轮廓区域始终启用（用户决定），空区域渲染为透明、无副作用

- [x] **修复眼睛偏右 + Tee 变小（tee_render 视觉回归）**
  - 根因 1（偏右）：tee_render 眼睛带方向偏移 `Dir.x*0.125*BaseSize`，且旧裁剪窗口 `(22,28,52,32)` 为猜测值 → 眼睛在 52×32 画布里偏右
  - 根因 2（变小）：`TEE_SIZE=64` 使身体仅 64px（旧渲染器身体填满 96×96 窗口）
  - 修复：`TEE_SIZE` 64→96（身体恢复填满 96×96）；`renderToPixmap` 改为把身体中心直接放画布中心（不再用 `GetRenderTeeOffsetToRenderedTee` 的 0.12 偏移）；独立眼睛像素图改回经典布局（52×32，左眼 (0,0)、右眼镜像 (16,0)，各 32×32，从皮肤区域采样），与旧渲染器完全一致、保证居中
  - 修改文件：`src/core/teedrawer.h/.cpp`

- [x] **修复 4K 皮肤区域采样错误（tee_render 集成时引入）**
  - `configureRegions` 之前直接用 256×128 参考坐标除以实际皮肤尺寸，导致 4K (4096×2048) 只采样左上角 96×96 的一角（身体 UV 变成 (0,0,0.023,0.047) 而非 (0,0,0.375,0.75)）
  - 修复：先按 `sx=skinW/256, sy=skinH/128` 把参考坐标缩放到实际皮肤，再归一化
  - 已用 256×128 (Tata) 与 4K (hollowknight) 两种皮肤定量验证：身体填满窗口、眼睛居中

- [x] **修复 4K 皮肤渲染锯齿（强摩尔纹/锯齿感）**
  - 根因：`QPixmapBackend::DrawQuad` 用 QPainter 双线性从完整 4096×2048 图集直接采样到 96×96 画布（身体区域 1536px→96px，16 倍缩小）。双线性大比例缩小时不对高频细节做面积平均，产生严重摩尔纹/锯齿（合成 1px 条纹测试：直接 4096→96 的亮度 TV=106.6，锯齿严重；逐级缩小到 256 后再渲染 TV=0，完全平滑）
  - 修复：加载皮肤时用 `downscaleToMaxDim()` 把大图集**逐级 2× 缩小**（每步双线性，等效低通滤波 / CPU 版 mipmap 生成）到最大边 ≤256 的"工作图集"，注册给渲染器采样；眼睛像素图同样从工作图集采样
  - 256×128 皮肤不受影响（已是工作分辨率，输出与修复前逐像素一致）
  - 修改文件：`src/core/teedrawer.h/.cpp`

- [x] **修复脚掌被压缩成正方形、且尺寸偏小（tee_render 遗留问题）**
  - 根因 1（正方形）：上游 DDNet `RenderTee7` 用 `w = h = BaseSize/2.1` 绘制脚掌，把 64×32（2:1）脚掌纹理纵向拉伸 2 倍成正方形（与 `GetRenderTeeFeetSize` 的 2:1 边界计算不一致）
  - 根因 2（偏小）：`BaseSize/2.1` 使脚掌只有身体 ~48% 宽（96 画布上 45.7px），而旧 Floatee 是 1:1 绘制脚掌纹理（64×32，约 2/3 身体宽）
  - 修复：渲染器改为 `w = (BaseSize/1.5) * FeetScale.x`、`h = w/2`，脚掌 2:1 且宽度恢复为纹理自然比例 `BaseSize*2/3`（64×32 @ BaseSize=96），与经典 Floatee 比例一致
  - 修改文件：`tee_render/src/tee_renderer.cpp`、`src/core/teedrawer.cpp`（TeeFoot 裁剪位置）

- [x] **tee_render 从 `extracted/` 移到项目根目录**
  - `extracted/tee_render/` → 根目录 `tee_render/`（`extracted/` 仅保留分析脚本、参考皮肤等素材，仍被 gitignore）
  - `CMakeLists.txt`：`add_subdirectory(extracted/tee_render)` → `add_subdirectory(tee_render)`
  - 更新 `tee_render/` 内部文档（PROGRESS/README/EMOTICON_RENDER）与 `example/main.cpp`、`example/render_skin.py` 中的路径注释
  - `.gitignore`：新增 `tee_render/build/`（tee_render 现纳入版本控制，构建产物除外）
  - 重新构建验证通过（产物位于 `build_check/tee_render/`）

- [x] **舍弃 Floatee 手工布局，改用 tee_render 原生布局**
  - 背景：此前为兼容旧外观，身体被强行放大填满 96×96 窗口、眼睛用独立 QLabel + 手工 52×32 像素图图层（旧 Floatee 布局）
  - 决定：完全采用 tee_render 原生布局——整个 Tee（身体+脚+眼睛）由管线渲染为**一张完整图像**，居中显示，脚在身体下方
  - 修改：
    - `teedrawer.cpp`：`renderToPixmap` 改用 `GetRenderTeeOffsetToRenderedTee` 原生居中；`TEE_SIZE` 96→72（原生居中下能完整放入 96×96 画布的最大尺寸）；`render()` 只渲染完整 Tee（含眼睛），删除旧的眼睛像素图/`TeeBare`/`TeeBody` 等成员
    - `floatee.cpp`：`BodyLabel` 直接显示完整 `Tee`；删除独立眼睛 QLabel 图层（`Eyes` 类）与手工 `eyePixmap()`；新增 `updateEyeFollow()`——用定时器跟随光标方向，把看向方向传给 `render()` 重新渲染，眼睛在脸部内滑动（tee_render 原生"眼睛跟随"），保留"光标靠近变笑脸"行为；右键循环 / 托盘眼睛菜单 / 换肤 / 调色全部改为重渲染完整 Tee
  - 验证：三种皮肤 Tee 质心 ≈ 画布中心 (48,48)，身体不再填满窗口，脚在下方，眼睛已渲染进 Tee

## 已知问题 / 待解决

### 中优先级

- [x] **`Floatee.pro` (qmake) 已过期**
  - 源文件列表、平台文件、资源文件均与 CMake 不同步
  - 已移除

- [ ] **`assets/main/` 资源可能已废弃**
  - `eyes.png`、`eyes_clever.png` 等看起来已不被 `teedrawer.cpp` 使用
  - 需要确认后清理

### 低优先级 / 已记录

- [ ] macOS 窗口侧边隐藏：AX API 对部分 Apple 原生应用/SwiftUI/Catalyst 窗口无效
- [ ] 皮肤文件尚未完全按 4K 模板标准化（部分皮肤元素位置不标准）
- [x] 右脚掌位置超出画布：旧渲染器遗留问题，tee_render 管线已按管线坐标绘制脚掌，不再超出

---

*最后更新: 2026-08-09*

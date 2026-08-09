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

- [x] **透明窗口皮肤褪色（Windows 合成 alpha 问题）——部分改善，仍有残留，待继续排查**
  - 现象：黑描边在白色背景下几乎纯黑，在彩色背景下变透明发灰（背景相关的合成异常）
  - 诊断（像素级验证）：渲染管线输出的 Tee 像素图**完全正确**（描边为不透明纯黑 + 正常半透明黑边缘），QPainter 合成、QLabel+透明窗口的 widget 树渲染也都正确 → 问题锁定在 **Qt→Windows 图层窗口（UpdateLayeredWindow）的 premultiplication 合成**环节
  - 已做的修复（部分改善，用户反馈"比之前好一些"，但彩色背景下描边仍有残留发灰）：
    - **移除 QLabel 中间层，改用窗口 `paintEvent` 直接绘制 Tee**（透明窗口的标准可靠模式，避免子控件合成路径的 alpha 处理差异）
    - 窗口增加 `Qt::WA_NoSystemBackground`
    - `refreshTranslucentDisplay()`（失焦/激活时触发）改为重新断言 `WA_TranslucentBackground` + 强制 `repaint()`，重新同步 premultiplied alpha 给 DWM
    - 所有显示更新（眼睛跟随/换肤/调色/变焦）改为 `update()` 触发 paintEvent
  - 修改文件：`src/ui/floatee.h/.cpp`
  - **待继续排查**：见下方"已知问题 / 待解决"中优先级条目

- [x] **完整 mip 链 + 按渲染比例选级 + 变焦支持（方案 2）**
  - **Mip 链**：`TeeDrawer` 用 `buildMipChain()` 从皮肤图集逐级 2× 低通缩小（起点 ≤1024，终点 ≥32）构建 `m_mips`，替代原来单一 256"工作图集"——对应游戏 `glGenerateMipmap` 的 CPU 版
  - **按比例选级**：`selectMip()` 选身体区域采样比 ≤1（最接近 1:1）的一级注册为纹理；放大超出链时回退最大级（轻微上采样仍平滑）——等价 `GL_LINEAR_MIPMAP_LINEAR`（未做三线性混合）
  - **动态尺寸**：`CANVAS_SIZE`/`TEE_SIZE` 常量改为成员 `m_canvasSize`/`m_teeSize`，新增 `setRenderScale(scale)`（更新画布/tee 尺寸 + 选 mip）；`renderToPixmap` 全部用动态尺寸
  - **变焦 UI**：托盘新增 Size 子菜单（50%~200%，QActionGroup），`switchSize()` 调用 `setRenderScale` + 调整窗口/BodyLabel + 重渲染，持久化到 `setup.json["Size"]`；眼睛跟随的"tee 内半径"随缩放等比放大
  - 验证：缩放 0.5×~2×（画布 48~192，teeSize 36~144）各档 tee 质心均 ≈ 画布中心，4K/256 皮肤一致
  - **mip 选择方向 bug 修复**：`selectMip()` 原从最大级（index 0）向下找"bodyPx ≥ teeSize"的第一项 → 永远选中最大级（4K@100% 错选 1024 图集，~5.3 倍缩小产生锯齿）。改为从**最小级向上**找最小满足项（最接近 1:1 且不上采样），4K@100% 正确选 256，与旧版一致
  - 验证：4K 各档选择 0.5→128 / 1.0→256 / 2.0→512，256 皮肤 100%→256，放大超链回退最大级
  - 修改文件：`src/core/teedrawer.h/.cpp`、`src/ui/floatee.h/.cpp`

- [x] **头顶表情（emoticon）——最终版：绘制进主窗口 + 多触发（2026-08-09 收尾）**
  - 复用 tee_render 提取的 `CEmoticonRenderer`（无状态、只输出一个四边形，走 `ITeeRenderBackend`）+ `assets/main/emoticons.png`（512×512，4×4 网格，16 个表情）
  - `src/ui/emoticonwindow.h/.cpp`：**从独立 QWidget 重构为 QObject 逻辑组件**（`loadAtlas` / `showEmoticon` / `renderFrame` / `frameChanged` 信号）。不再创建任何 OS 窗口——因为本机**第二个 `WS_EX_LAYERED` 窗口永远不会被 DWM 合成**（PrintWindow 能看到其自身表面内容 3312px，但 CopyFromScreen 屏幕 diff=0；试过无父顶层、启动即 show、去 `WS_EX_TRANSPARENT`、`WA_TranslucentBackground` 切换，全部无效）
  - 主窗口 `paintEvent` 在画完 Tee 后叠加表情帧：窗口高度向上扩展 `headroom`（`ceil(87*Scale - TeePos.y)`），`resize` 时同步；`frameChanged` → `update()` 驱动 60fps 动画
  - 触发（保持不变）：**周期随机**（每 10s，50%）、**拖拽开始**、**摸头**（进 happy 区边沿触发爱心）、**切换眼睛**（右键循环 / 托盘菜单）、**切换大小**
  - **QPixmap 隐式共享分离 bug**：`m_backend.target = target` 后绘制会分离、target 保持空白 → 需 `target = m_backend.target` 读回（与 `TeeDrawer` 的 `out = m_backend.target` 同理）
  - **qrc 别名根因 bug（本次定位）**：`Floatee.qrc` 中 `<file>assets/main/emoticons.png</file>` 没有 `alias` → 资源路径实际是 `:/main/assets/main/emoticons.png` 而非 `:/main/emoticons.png`，运行时 `QPixmap(":/main/emoticons.png")` 为 NULL → `loadAtlas` 失败 → 表情 quad 从未绘制（帧全透明）。皮肤能用是因为它显式写了 `alias="Tata.png"`。修复：给 `/main` 下所有文件补上 `alias`（同 qrc 其余资源的既有约定）
  - 验证：应用内把 paintEvent 的表情帧 dump 成 PNG 逐像素分析——修复前 0 不透明像素，修复后弹出动画帧 12→51px 递增 ✓；DrawQuad 日志确认 512×512 表情图集 quad（0→36px 弹出）✓
  - 修改文件：`Floatee.qrc`、`src/ui/emoticonwindow.h/.cpp`、`src/ui/floatee.h/.cpp`

- [x] **鼠标滚轮缩放（每次一档，不灵敏 + 以鼠标为中心）**
  - 宠物窗口上滚动滚轮：放大/缩小，**一个滚轮事件只移动一个等级**——特意不用 `delta/120` 跳多级，即使快速/高分辨率滚轮在一次事件里打包了多格 delta 也只缩一档
  - 档位与 Size 菜单一致：**50%~200% 每 10% 一档，共 16 档**（50/60/70/80/90/100/110/120/130/140/150/160/170/180/190/200%）；当前比例先吸附到最近档位再移动一格（保证永远落在菜单档位上），到 50% / 200% 边界不再继续
  - **档位单一来源**：`kZoomLevels` 静态表（`floatee.cpp` 顶部），Size 菜单与滚轮 `zoomSize` 共用，避免两份硬编码列表不同步
  - 向上滚放大、向下滚缩小；缩放后同步 Size 菜单勾选、持久化 `setup.json["Size"]`、更新托盘/窗口图标
  - **以鼠标为缩放中心（滚轮路径）**：`resize()` 默认保持左上角固定，缩小后光标会落到 Tee 绘制区外、无法连续缩放。修复：缩放前记录鼠标在窗口内的偏移 `local0`，按 `newSize/oldSize` 比例缩放该偏移得到新窗口位置，使光标始终指向 Tee 上同一点（tee 与窗口近似线性缩放，headroom ≈ 41·scale）。菜单切换尺寸仍保持左上角锚定（不移动窗口）
  - **快速缩放抽搐/残影修复（三轮）**：
    1. 原 `resize()`（左上角锚定）后 `move()` 到鼠标锚点 → 改为滚轮路径**一次 `setGeometry()`**（位置+大小单次设置）
    2. 仍有残留——**几何改变后、新尺寸 Tee 重渲染前 paintEvent 把旧尺寸 Tee 画进新窗口**产生错位帧 → 用 `setUpdatesEnabled(false)` 把"几何变化 + 重渲染"包成原子操作，恢复后一次 `update()` 只合成最终帧
    3. 仍有快速残影——尝试**滚轮防抖合并**（wheelEvent 累积步数 + 100ms 定时器一次跳到最终档）：**反而加剧抖动（一次大跳变更明显），已撤销**，恢复逐级缩放
    4. 改用**同步 repaint()**：`setGeometry` 几何立即生效、内容异步 `update()` 上传导致"新几何+旧内容"窗口期（`setUpdatesEnabled(false)` 反而拉长它）。`repaint()` 在缩放函数返回前完成 paintEvent + UpdateLayeredWindow 上传；并暂停 EyeFollowTimer。仍有抖动
    5. **内置 100ms 冷却（用户建议）**：`wheelEvent` 加 `m_zoomCooldown`（QElapsedTimer）节流。无效
    6. **诊断（关键）**：日志证明锚定位置计算完全正确（请求==实际 pos），且鼠标固定时位置序列确定 → 抖动不是计算/系统调整问题。用户观察到"窗口异常位移（锚定导致窗口随缩放大幅移动）+ 抖动帧持续 ~100ms + 录屏录不到"→ 定位为 **DWM 图层窗口"几何变化 + 内容重传"的非原子合成**：SetWindowPos 同步生效、UpdateLayeredWindow 异步上传，其间 ~100ms 合成"新几何+旧内容"错位帧；录屏（抓窗口内容/最终合成）采不到，人眼能看到。纯内容更新（眼睛/表情/拖拽）与纯移动都不抖
    7. **根治方案（当前）**：**窗口几何永不变化**——窗口固定为 200% 档尺寸（192×275），缩放只重渲染 tee 并移动其在窗口内的位置 `m_teePos`（**内容锚定**：鼠标指向的 tee 点保持在其下方），变成纯内容更新，机制上杜绝几何-内容非原子抖动，同时消除"窗口异常位移"。代价：窗口大小不再随缩放变化（缩小后 tee 周围透明区域较大）。`emoticonHeadroom`/`teeTeePos` 辅助函数删除（窗口固定不再需要）。用户确认彻底根治后，**100ms 冷却已移除**（纯内容更新下不再需要限频）
  - 重构：核心缩放逻辑提取为 `applySizeScale(double, bool anchorAtCursor=false)`（返回是否真正改变），`switchSize`（菜单，不锚定）与 `zoomSize(step)`（滚轮，锚定光标）共用；滚轮缩放**不**触发随机表情（避免连续滚动刷表情），菜单切换仍触发
  - 修改文件：`src/ui/floatee.h/.cpp`

### 平台验证发现（2026-08-09，本机 Windows）

- **`CopyFromScreen` 捕获为全黑**：本机当前屏幕捕获返回整屏纯黑（锁屏/显示器关闭/会话断开），因此"屏幕 diff=0"这类验证全部无效，不能作为"没显示"的证据
- **`PrintWindow` 返回陈旧表面**：对图层窗口只反映**初始合成内容**，不反映后续 paintEvent 的实时更新（前后各时段采样均相同，尽管 paintEvent 在持续绘制红色闪烁测试 + 表情）→ 实时内容验证只能靠**应用内 dump 帧 PNG + 像素分析**
- **第二个 `WS_EX_LAYERED` 窗口永不合成**：表情必须绘制进主 Floatee 窗口（主窗口的实时合成是正常的——用户可见眼睛跟随/拖拽等）

- [x] **眼睛跟随改为"随鼠标距离变化"（原版 Floatee 手感，管线内实现）**
  - 原版方案：眼睛偏移随鼠标与 tee 的距离非线性变化（三角数序列压缩 ≈√距离，钳制 ±15 椭圆），鼠标在 tee 上时偏移≈0（眼睛居中）
  - tee_render 原方案：`Offset = Dir*0.125*BaseSize`，Dir 为单位向量 → 偏移幅度恒定，不随距离变化
  - 改进：管线新增 `m_Skin6EyeOffsetScale`（`STeeRenderInfo`），眼睛偏移公式改为 `Dir * 0.125 * BaseSize * scale`（垂直同理），方向仍由 Dir 控制、眼间距不变
  - `TeeDrawer::render()` 新增 `eyeOffsetScale` 参数；Floatee 按鼠标到 tee 中心的距离计算：tee 内（≤35px）为 0（眼睛居中），远处用**非线性 √ 型压缩**（`offset_px ≈ √(dist/6)`，同原版 Floatee），中远距离系数更小、约 800px 才饱和到 1.2（≈10.8px），避免过早撞"边界墙"
  - 验证：scale=0 眼睛质心 (47.5,48.5) 居中；scale=1.3 右移 11.6px（理论 11.7px）✓
  - **近区微调（2026-08-09）**：鼠标在 Tee 身上时眼睛不再完全居中——tee 内（len ≤ 35·SizeScale）改为 0→0.45 的平滑斜坡（光标在中心=0，到 tee 边缘≈4px 偏移），远处曲线从 0.45 继续 √ 增长、cap 仍 1.2；宠物在鼠标悬停时眼睛也跟随，更有"活着"的感觉
  - **眼间距恒定修复**：管线眼间距原为 `(0.075 - 0.010*|Dir.x|)*BaseSize`，随看向方向水平分量变化（鼠标在 tee 中心时方向回退 `(1,0)` 使间距窄 0.72px，划过中心会跳变）。新增 `m_Skin6EyeSeparationDirectionScale`（默认 1.0 保持 DDNet 行为），Floatee 设为 0 → 眼间距恒定
  - 验证：dir=(1,0) 与 dir=(0,-1) 眼间距 9.39/9.41px，差值 0.02px ✓
  - 修改文件：`tee_render/include/tee_render_info.h`、`tee_render/src/tee_renderer.cpp`、`src/core/teedrawer.h/.cpp`、`src/ui/floatee.h/.cpp`

- [x] **启用 tee_render 的 walk 动画（拖拽行走）**
  - `TeeDrawer::render()` 新增 `walkPhase` 参数（[0,1) 走一个 walk 循环，<0 为 idle）：`walkState.Set(ANIM_BASE, 0)` + `Add(ANIM_WALK, phase, 1)`，完全按 DDNet 方式驱动
  - `renderToPixmap` 改为接收 `CAnimState*`，渲染与居中偏移都使用该动画状态
  - Floatee：拖拽宠物时按水平位置驱动相位（DDNet 公式 `fmod(x,100)/100`），脚掌随拖拽迈步、身体轻微起伏；松手回到 idle
  - `updateEyeFollow()` 的去重判断加入 walk 相位，避免拖拽时漏渲染
  - 修改文件：`src/core/teedrawer.h/.cpp`、`src/ui/floatee.h/.cpp`
  - 验证：walk 0.25/0.75 帧脚掌收窄、身体抬高 1px，与 idle/0/0.5 帧不同

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
- [x] **失焦周期性闪烁（透明/不透明来回切换）——已修复（2026-08-09）**
  - 现象：焦点在 VS Code 等 Chromium/Electron 窗口时，宠物每隔几秒在透明/不透明间闪烁
  - 根因：这类应用周期性发出激活相关事件；原 `refreshTranslucentDisplay()` 在每次 `ActivationChange` 都重复断言 `WA_TranslucentBackground`，使 Qt 重新应用原生 `WS_EX_LAYERED` 样式（移除再添加）→ 透明/不透明闪烁
  - 修复：`changeEvent` 仅在激活状态**真正翻转**（`isActiveWindow()` 变化）时刷新；`refreshTranslucentDisplay` 移除重复断言，只做轻量 `repaint()`
  - 修改文件：`src/ui/floatee.h/.cpp`

- [ ] **透明窗口合成：彩色背景下黑描边发灰（残留，暂缓）**
  - 现象：黑描边在白色背景下正常，彩色背景下变透明发灰（背景相关的合成异常）
  - 已定位到 Qt→Windows 图层窗口（`UpdateLayeredWindow`）的 premultiplication 合成环节；渲染管线输出经像素级验证完全正确
  - 已尝试：移除 QLabel 改用 `paintEvent` 直接绘制、`WA_NoSystemBackground`、失焦时轻量 `repaint()` —— **部分改善，仍有残留；用户暂缓处理**
  - 后续仍可尝试：`QWidget::render` 预合成、直接操作窗口句柄 `SetLayeredWindowAttributes`、或换用非图层窗口方案
- [ ] **`assets/main/` 资源部分可能已废弃**
  - `emoticons.png` 现已被表情功能使用（qrc `/main` 下已全部补 alias）
  - `eyes.png`、`eyes_clever.png` 等看起来已不被 `teedrawer.cpp` 使用，需要确认后清理

### 低优先级 / 已记录

- [ ] macOS 窗口侧边隐藏：AX API 对部分 Apple 原生应用/SwiftUI/Catalyst 窗口无效
- [ ] 皮肤文件尚未完全按 4K 模板标准化（部分皮肤元素位置不标准）
- [x] 右脚掌位置超出画布：旧渲染器遗留问题，tee_render 管线已按管线坐标绘制脚掌，不再超出

---

*最后更新: 2026-08-09（表情收尾 + 鼠标滚轮缩放）*

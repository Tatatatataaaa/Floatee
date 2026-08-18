---
name: floatee-qt-gotchas
description: Floatee (Qt6/C++ 桌面宠物) 的 Qt/Windows 平台陷阱与渲染验证方法。遇到屏幕抓取全黑、窗口不合成、qrc 资源找不到、QPixmap 绘制异常、缩放抖动、HiDPI 模糊等疑难问题时按此排查，避免重复推导。
---

# Floatee Qt/Windows Gotchas

Floatee 是 Qt6/C++ 透明无边框桌面宠物（`src/ui/floatee.cpp` + `tee_render/` 软件渲染）。以下是从真实调试中沉淀的陷阱与验证方法。

## 实时渲染验证（最重要）

- **`CopyFromScreen` 返回全黑**（锁屏/断会话下）：屏幕 diff 无效，不能当作"没显示"的证据。
- **`PrintWindow` 对图层窗口返回陈旧初始表面**：不反映后续 `paintEvent` 实时更新。
- **第二个 `WS_EX_LAYERED` 窗口永不 DWM 合成**：表情/气泡等叠加必须画进主 Floatee 窗口的 `paintEvent`（`EmoticonWindow` 是 QObject，由主窗口调用其 `renderFrame`）。
- ✅ **可靠验证 = 应用内 dump 帧 PNG + PowerShell 像素分析**。

## qrc 资源

- qrc 无 `alias` 时资源路径保留子目录：`assets/main/x.png` 是 `:/main/assets/main/x.png` 而非 `:/main/x.png`。皮肤用 `alias="Tata.png"` 所以正常。
- **qrc 新文件必须显式加 `alias`**，否则路径会变。

## QPixmap 隐式共享

- `backend.target = pix` 后，QPainter 绘制会 **detach**，必须 `pix = backend.target` 读回（`TeeDrawer::renderLayers` / `EmoticonWindow::renderFrame` 都是这个模式）。
- `QPixmap::fromImage` 会**丢失 devicePixelRatio 标记**——feather 后要重新 `setDevicePixelRatio(dpr)`（Tee 本体与表情气泡都踩过）。

## 缩放抖动（已根治，勿回退）

- 根因：DWM 图层窗口"几何变化+内容重传"非原子合成（约 100ms 显示"新几何+旧内容"错位帧）。
- **根治方案**：窗口固定 `kWinW×kWinH`（192×275，200% 档尺寸），缩放只重渲染 tee 并移动窗口内 `m_teePos`（内容锚定），纯内容更新不抖。
- 已失败方案（勿再试）：resize+move 两步、setGeometry 一次、setUpdatesEnabled(false)、hide/show、100ms 冷却、滚轮防抖合并。

## 渲染平滑

- **SSAA 2x** 超采样（渲染到 2 倍画布再双线性缩回，`selectMip` 按渲染尺寸选级）。
- **alpha 羽化** `TeeDrawer::featherAlpha`：3×3 邻域均值只作用半透明边缘、`qMax` 向外扩散。SSAA 单独无法突破 1px 图集边缘，必须羽化在目标分辨率生成过渡。
- 羽化强度 `setFeatherStrength 0/1/2`（托盘 Feather 菜单，持久化 `default.json["Feather"]`）。

## Mip 链

- `selectMip` 从**最小级向上**找 `bodyPx >= teeSize`（反向会 4K 错选 1024 级）。

## 眼睛

- 眼间距恒定：`m_Skin6EyeSeparationDirectionScale = 0`。
- 眼睛距离非线性：`eyeScale = sqrt((len - 35*SizeScale)/6)/9`，cap 1.2。
- Tee 内（`len <= 35*SizeScale`）：不再居中，0→0.45 平滑斜坡（抚摸感）。

## IME 输入法（自绘输入框）

- 自绘 QMainWindow 即使 `ImEnabled=true` 也唤不起系统输入法（TSF 只在标准控件上可靠）。
- ✅ **方案：透明 `QLineEdit` 代理**（`m_imeEdit`）承载输入法焦点，`textChanged`/`returnPressed` 同步到自绘框。
- Qt 6 无 `QInputMethod::preeditString` / `QLineEdit::preeditTextChanged` / `setCursorWidth`——用 `eventFilter` 捕获 `QInputMethodEvent` 读 preedit。
- QLineEdit 内置光标要拦截 `QEvent::Paint` 吞掉（否则双闪烁光标）。

## HiDPI

- `TeeDrawer::setDevicePixelRatio(dpr)`：渲染像素 = 逻辑尺寸 × dpr，`drawPixmap` 仍按逻辑坐标（位置不变）。
- `opaqueRect()` 是**物理像素**，`clampTeePos`/`hitTestTee`/托盘图标裁剪必须除以 dpr 转逻辑。
- 窗口拖到不同 DPI 屏幕 → `QEvent::ScreenChangeInternal` → `applyTeeDpr()` 重渲染。

## 工具注意

- 本仓库记忆 `floatee-notes.md` 用 `str_replace` 会损坏（丢行/undefined）——更新用 delete+create 或 insert。
- 构建务必先杀进程（LNK1104），用绝对路径（终端 cwd 可能不在仓库根）。

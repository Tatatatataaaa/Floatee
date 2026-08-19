# Floatee — Agent 工作区指令

Floatee 是一个 Qt 6 / C++ 桌面宠物（Tee），带联机功能（`server/` Node.js 服务端）。本文件让 agent 快速掌握构建、测试、部署的关键路径。

## 构建（Windows，MSVC）

**必须先停掉运行中的 Floatee 进程**，否则链接报 `LNK1104`（exe 被锁）：

```powershell
Get-Process Floatee -ErrorAction SilentlyContinue | Stop-Process -Force
```

然后构建（命令以仓库根为工作目录，终端 cwd 可能在别处时先 `cd` 到仓库根）：

```powershell
cmake --build build_check --config Release --target Floatee
```

产物：`build_check\Release\Floatee.exe`

运行验证：

```powershell
Start-Process "...\build_check\Release\Floatee.exe"
Get-Process Floatee   # 应存在，正常启动
```

## 测试配置

- 运行配置在 `%APPDATA%\Floatee`：`default.json`（默认）、`default_<名字>.json`（多实例，`--profile=<名字>` 启动）、共享 `skins/` 皮肤库
- 运行时 log：`%APPDATA%\Floatee\logs\floatee_runtime.log`（每分钟状态 + 每 10 分钟系统时间）

## 服务端（server/）

```powershell
cd server; npm test          # 单测（node:test）
node src/index.js            # 启动（ws:9001 / tcp:8764 / admin:8766）
```

- `server/config.json` 含敏感信息（adminKey），**不入库**；部署时复制 `config.example.json`
- Admin 密钥通过环境变量 `ADMIN_KEY` 提供

## 分支与改动约定

- 当前开发分支：`online`（联机功能；屏蔽了 WSH/Eye Care，勿合回 main）
- 渲染层：`tee_render/`（零依赖软件渲染，src/core/teedrawer.cpp 是其 Qt 后端）
- 渲染验证：应用内 dump 帧 PNG + PowerShell 像素分析（CopyFromScreen/PrintWindow 不可靠，见 floatee-qt-gotchas skill）

## 跨平台

- Windows/macOS/Linux 交叉改动在 `CMakeLists.txt` 的条件分支里，改完务必回归 Windows 构建
- HiDPI：`TeeDrawer::setDevicePixelRatio`（1 渲染像素 = 1 物理像素）；opaqueRect 是物理像素，clamp/hitTest 需除以 dpr

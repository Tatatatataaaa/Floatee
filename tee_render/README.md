# tee_render — 提取的 Tee 渲染管线

从 QmClient（基于 DDNet / TaterClient）中提取的 **Tee 渲染管线**，作为**零依赖的独立 C++17 模块**，可用于任何其他项目。

核心目标：把 `CRenderTools::RenderTee / RenderTee7` 的渲染逻辑、`CAnimState` 动画系统和皮肤数据解耦出来，**不依赖 DDNet 引擎**（不依赖 `IGraphics`、`generated/*.h`、`base/*.h`）。

## 目录结构

```
tee_render/
├── include/
│   ├── tee_math.h          # 极简数学（vec2 / ColorRGBA / mix / clamp）
│   ├── tee_types.h         # 表情、皮肤部件、动画关键帧、精灵枚举
│   ├── tee_anim.h          # 动画状态 + 预设动画数据
│   ├── tee_skin.h          # 皮肤纹理结构（抽象纹理句柄）
│   ├── tee_render_info.h   # STeeRenderInfo + 渲染 flags + 皮肤切换过渡
│   ├── tee_backend.h       # 抽象渲染后端接口（替代 IGraphics）
│   ├── tee_renderer.h      # 核心渲染器
│   └── tee_emoticon.h      # 头顶表情渲染器（CEmoticonRenderer）
├── src/
│   ├── tee_anim.cpp
│   ├── tee_renderer.cpp
│   └── tee_emoticon.cpp
├── example/                # 示例：SVG 输出 + HTML/Canvas 动态演示
│   ├── main.cpp                      # C++ 示例：输出 quads.txt + skin_render.svg
│   ├── render_skin.py                # PIL 光栅化：quads + 皮肤图 → skin_render.png
│   ├── emoticon_popup_demo.html      # 纯 HTML/Canvas 动态演示（浏览器打开）
│   └── check_atlas.py                # 调试用：把 emoticons.png 切成 16 个细胞
├── CMakeLists.txt
├── README.md
└── EMOTICON_RENDER.md      # 表情渲染管线提取文档（含迁移公式）
```

## 与原管线的映射

| 本模块 | 原 DDNet / QmClient 源码 |
|--------|--------------------------|
| `CTeeRenderer::RenderTee` | `CRenderTools::RenderTee`（`src/game/client/render.cpp`） |
| `CTeeRenderer::RenderTee7` | `CRenderTools::RenderTee7`（protocol7 六部件皮肤渲染） |
| `CTeeRenderer::RenderTeeWithSkinChangeTransition` | `CRenderTools::RenderTeeWithSkinChangeTransition`（QmClient 皮肤切换过渡） |
| `CTeeRenderer::GetRenderTee*` 系列 | `CRenderTools::GetRenderTeeBodyScale/FeetScale/BodySize/...` |
| `CAnimState` | `CAnimState`（`src/game/client/animstate.h/cpp`） |
| `s_aAnimations[]` + `EAnimIndex` | 生成自 `datasrc/content.py` 的 `generated/client_data.h`（base/idle/inair/sit/walk/run/swing） |
| `STeeRenderInfo` | `CTeeRenderInfo`（`src/game/client/render.h`） |
| `SSixupSkin` | `CTeeRenderInfo::CSixup` |
| `STeeSkinTextures` | `CSkin::CSkinTextures`（protocol 6，保留备用） |
| `ITeeRenderBackend` | `IGraphics` 的绘制相关子集 |
| `SSpriteRegion` + `ResolveSprite` | `Graphics()->SelectSprite7(...)` |
| `STextureHandle` | `IGraphics::CTextureHandle` |
| `SKINPART_*` | `protocol7::SKINPART_*`（`datasrc/seven/network.py`） |
| `ETeeSprite` | `client_data7::SPRITE_TEE_*`（`datasrc/seven/content.py`） |
| `CEmoticonRenderer` | `CPlayers::RenderPlayer` 的表情绘制（`src/game/client/components/players.cpp`） |
| `example/emoticon_popup_demo.html` | 纯前端 pop-in 动画演示 |

## 快速开始

```bash
cmake -S . -B build
cmake --build build
./build/example/tee_render_example    # 生成 tee_render.svg
```

打开 `tee_render.svg` 即可看到 5 个 Tee：
1. idle 站立（happy 表情，朝右）
2. walk 走路动画（t=0.5）
3. 皮肤切换过渡（ghost-pop，progress=0.5）
4. 果冻变形参数（身体/脚拉伸+旋转，模拟高速移动）
5. 朝左的 angry Tee（验证眼睛偏移与朝向）

## 在你的项目中接入

1. **实现 `ITeeRenderBackend`**：把 `STeeQuad` 翻译成你的图形 API（OpenGL/Vulkan/DirectX/软件渲染）。
   - `BeginTee()` / `EndTee()` 每个 Tee 调用一次（可用于 flush 批处理）。
   - `DrawQuad()` 收到带位置/尺寸/旋转/颜色/UV/翻转的四边形。
2. **填充 `STeeRenderInfo`**：
   - `m_Size = 64.0f`（游戏内标准尺寸）。
   - `m_aSixup[0]` 填六部件纹理句柄（BODY/MARKING/DECORATION/HANDS/FEET/EYES）与颜色。
   - 可选：`m_HatTexture` / `m_BotTexture`（帽子 / bot 皮肤）。
3. **准备精灵区域**：默认每个皮肤部件纹理按整张采样（UV 0..1）。如果你的部件纹理是图集，用 `CTeeRenderer::SetSpriteRegion(sprite, region)` 覆盖 UV。
4. **播放动画**：
   ```cpp
   CAnimState State;
   State.Set(&s_aAnimations[ANIM_WALK], t);   // t 在 [0,1] 循环
   Renderer.RenderTee(&State, &Info, EMOTE_NORMAL, vec2(1,0), Pos, 1.0f);
   ```
5. **（可选）果冻变形**：把 `BodyScale / FeetScale / BodyAngle / FeetAngle` 传给 `RenderTee`，即可复现 QmClient 的果冻 Tee 效果（原始算法在 `src/game/client/components/qmclient/jelly_tee.cpp`）。

## 设计要点

- **纯 C++17**，无任何第三方依赖，头文件即 API。
- **纹理解耦**：用 `uint32_t` 句柄表示纹理，宿主自行映射到真实 GPU 纹理。
- **精灵图集支持**：`SSpriteRegion` 提供 UV，默认全纹理；`SetSpriteRegion` 可覆盖。
- **保留 QmClient 增强**：皮肤切换过渡（5 种类型）和果冻变形参数接口都已包含；色相循环（`QmApplyTeeHueCycle`）与战列发光属于玩家层，可在宿主里用 `STeeRenderInfo::m_aColors` 实现，未内嵌。
- **平滑缩放**：原游戏对皮肤/表情 sprite 默认生成 mipmap 链，并用 `GL_LINEAR_MIPMAP_LINEAR` / `GL_LINEAR` 三线性过滤。本模块通过 `ITeeRenderBackend` 暴露纹理创建 flags，宿主可在 GPU 后端实现相同策略。

## 未包含（有意为之）

- **protocol 6（旧版 `RenderTee6`）** 渲染路径——提取面向现代 protocol7 六部件皮肤；如需旧版可参考 `src/game/client/render.cpp` 中 `RenderTee6`。
- **皮肤加载/缓存**（`skins.cpp` / `gameclient.cpp`）——属于引擎资源管理，与本管线正交。
- **皮肤切换状态机**（`CClientData::UpdateSkinChangeTransition`）——提供了纯函数的过渡 blend 计算，宿主自行管理进度。

## 许可

上游代码基于 DDNet，遵循 zlib/libpng 许可（`license.txt` 位于 QmClient 根目录）。

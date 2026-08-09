# tee_render 提取与皮肤渲染进度

> 更新日期：2026-08-08
> 位置：`tee_render/`（独立零依赖 C++17 模块）

本文档记录从 QmClient 提取 Tee 渲染管线、并用真实皮肤图（`_ghostjtj.png`、`hollowknight.png`）复现渲染的进度与结论。源码映射与模块设计见 `README.md`。

---

## 1. 总体目标

把 QmClient（DDNet / TaterClient）的 Tee 渲染管线提取为独立模块，供其他项目使用：

- 不依赖 DDNet 引擎（不依赖 `IGraphics`、`generated/*.h`、`base/*.h`）
- 纯 C++17、零第三方依赖
- 抽象 `ITeeRenderBackend` 输出四边形，宿主自行对接图形 API
- 保留 QmClient 增强：皮肤切换过渡、果冻变形参数接口

**当前状态**：管线提取完成并编译通过；已用 4K 皮肤 `hollowknight.png` 在 4K 画布上渲染出单个放大的 Tee，结果正常。

---

## 2. 目录结构

```
tee_render/
├── include/
│   ├── tee_math.h          # 极简数学（vec2 / ColorRGBA / mix / clamp）
│   ├── tee_types.h         # 表情、皮肤部件、动画关键帧、精灵枚举（ETeeSprite）
│   ├── tee_anim.h          # 动画状态 CAnimState + 预设动画数据
│   ├── tee_skin.h          # 皮肤纹理结构（抽象纹理句柄 STextureHandle）
│   ├── tee_render_info.h   # STeeRenderInfo + flags + 皮肤切换过渡
│   ├── tee_backend.h       # 抽象渲染后端接口 ITeeRenderBackend
│   └── tee_renderer.h      # 核心渲染器 CTeeRenderer + SSpriteRegion
├── src/
│   ├── tee_anim.cpp        # 动画求值 + 预设数据（来自 datasrc/content.py）
│   └── tee_renderer.cpp    # RenderTee / RenderTee7 移植实现
├── example/
│   ├── main.cpp                      # 导出 quads.txt + skin_render.svg 的示例
│   ├── render_skin.py                # PIL 光栅化 → skin_render.png
│   ├── emoticon_popup_demo.html      # 纯 HTML/Canvas 表情 pop-in 动画演示
│   └── check_atlas.py                # 调试用：把 emoticons.png 切成 16 个细胞
├── CMakeLists.txt
├── README.md
├── PROGRESS.md             # 本文档
└── 生成产物
    ├── quads.txt           # 管线真实输出的四边形列表
    ├── skin_render.svg     # 可查看的 SVG
    └── skin_render.png     # PIL 光栅化的 PNG（当前 3840×2160）
```

**渲染链路**：`main.cpp`（管线计算四边形）→ `quads.txt` + `skin_render.svg` → `render_skin.py`（PIL 采样皮肤图 + UV 裁剪 + 旋转/翻转/合成）→ `skin_render.png`。

**quads.txt 格式**：`tex_id cx cy w h rot r g b a u0 v0 u1 v1 flip`

- `tex_id`：1 = 皮肤图集（`hollowknight.png`）
- `cx/cy` 中心、`w/h` 尺寸、`rot` 弧度旋转、`r g b a` 颜色
- `u0 v0 u1 v1`：图集 UV 区域、`flip`：水平镜像（右眼用）

---

## 3. 4K 皮肤模板布局（protocol-7，4096×2048）

两张已处理的皮肤（`_ghostjtj.png`、`hollowknight.png`）都基于同一张 4K protocol7 模板。各区域（`皮肤模板区域切分总结.md`）：

| 区域 | 用途 | 坐标 (x, y, w, h) | 尺寸 |
| --- | --- | --- | --- |
| A | 头部 Base | (0, 0, 1536, 1536) | 3×3 tile |
| B | 头部 Outline | (1536, 0, 1536, 1536) | 3×3 tile |
| C | 手 Base | (3072, 0, 512, 512) | 1×1 tile |
| D | 手 Outline | (3584, 0, 512, 512) | 1×1 tile |
| E | 脚 Base | (3072, 512, 1024, 512) | 2×1 tile |
| F | 脚 Outline | (3072, 1024, 1024, 512) | 2×1 tile |
| G1 | 普通眼 | (1024, 1536, 512, 512) | 1×1 tile |
| G2 | 生气眼 | (1536, 1536, 512, 512) | 1×1 tile |
| G3 | 蠢拙眼 | (2048, 1536, 512, 512) | 1×1 tile |
| G4 | 快乐眼 | (2560, 1536, 512, 512) | 1×1 tile |
| H | 呆呆眼 | (3584, 1536, 512, 512) | 1×1 tile |

> 注意：手（C/D 区域）在 protocol-7 中**存在且会被渲染**，但它**不在 Tee 本体渲染函数 `RenderTee7` 中绘制**，而是在玩家渲染层 `CPlayers::RenderHand`（`src/game/client/components/players.cpp`）里、渲染武器/钩子时按瞄准方向动态单独绘制（见 §8）。本提取管线只移植了 `RenderTee7`，未移植 `RenderHand`，所以示例中不渲染手——这是提取范围的限制，不是 protocol7 本身没有手。

---

## 4. 进度时间线

### 4.1 管线提取（已完成）

- 移植 `CRenderTools::RenderTee / RenderTee7` → `CTeeRenderer::RenderTee / RenderTee7`
- 移植 `CAnimState` + 预设动画数据（base/idle/inair/sit/walk/run/swing）
- 抽象 `ITeeRenderBackend`、`SSpriteRegion`（精灵只提供 UV 区域）、`STextureHandle`
- 保留 QmClient 增强：`RenderTeeWithSkinChangeTransition`（5 种 blend 类型）、果冻变形参数
- 验证：`cmake -S . -B build -G "MinGW Makefiles"` 构建成功，示例输出 5 个 Tee 的 SVG

### 4.2 `_ghostjtj.png`（protocol7 4K 模板的 1/16 缩小版，256×128）

**关键识别**：该图是 4K 模板的 1/16 缩小版（256×128，每格 32px），不是 protocol6 单图。

已完成的适配：

- 全部部件（body/eyes）指向同一张皮肤图纹理，用 `SetSpriteRegion` 设不同 UV
- **skin6 双眼渲染**：每个眼睛纹理 tile 是单眼正方形，需要绘制两次；第二只眼水平镜像（`flip=true`）。眼宽 = `BaseSize * 0.40f`
- **脚部透明**：`SKINPART_FEET` 纹理保持无效 → 管线自动跳过脚 quad（`feet quads = 0`），不再用白色占位
- **眼间距系数** `m_Skin6EyeSeparationScale`：
  - 原始 QmClient 公式：`EyeSeparation = (0.075 - 0.010 * |Dir.x|) * BaseSize * BodyScale.x`
  - ghost 皮肤可见眼睛轮廓较宽，`1.0f` 会重叠 → 示例用 `1.5f`
  - 结论：ghost 皮肤上 `1.0f` 为 QmClient 原始间距（旧版对照图 `skin_render_previous_spacing*.png`），`1.5f` 为当前最终版

### 4.3 `hollowknight.png`（完整 4K 模板，4096×2048）

把示例切换到完整 4K 皮肤：

- 纹理文件、UV 区域全部改为 4K 坐标（见 §3）
- **启用脚部**：该皮肤有脚（E/F 区域有内容），设置 `SKINPART_FEET` 纹理并配置脚区域 UV
- **眼间距用 `1.0f`**：hollowknight 的眼睛可见轮廓（约 192px/512px tile）比 ghost（约 13px/32px tile）窄，`1.0f` 不会重叠，符合 QmClient 原始间距
- 渲染脚本改为基于脚本目录解析皮肤图 / 输出路径，兼容不同 CWD

### 4.4 已修复的问题

| 问题 | 根因 | 修复 |
| --- | --- | --- |
| 第二个 Tee 的脚偏移到眼睛处 | 示例只 `Set(ANIM_WALK)`，没有叠加 base 姿态（base 的脚偏移为 `(0,10)`） | 改为 `Set(ANIM_BASE, 0)` 后再 `Add(ANIM_WALK, t, 1)` |
| 渲染图不是 4K | 画布固定 `960×400` | 画布与 Python 输出均改为 `3840×2160` |
| 4K 画布上 Tee 太小 | `m_Size = 64` | 单独渲染时 `m_Size = 512`，身体 `512×512` 居中 |
| 无效 hand 精灵调用 | protocol-7 渲染器不单独渲染手，且枚举里没有 `TEE_SPRITE_HAND_*` | 移除无效调用，手由 body 纹理提供 |
| 脚掌被拉伸成正方形 | 上游 DDNet `RenderTee7` 用 `w = h = BaseSize/2.1` 画脚，把 64×32（2:1）脚掌纹理纵向拉伸 2 倍（与 `GetRenderTeeFeetSize` 的 2:1 边界不一致） | 改为 `h = (BaseSize/1.5) * FeetScale.y / 2`，脚掌按 2:1 自然宽高比渲染，且宽度恢复为纹理自然比例 `BaseSize*2/3`（64×32 @ BaseSize=96），与经典 Floatee 比例一致（2026-08-09） |

---

## 5. 当前渲染结果（已验证）

单个站姿 Tee（happy、朝右），画布 `3840×2160`，`m_Size = 512`：

| 部件 | 尺寸 | 中心位置 | 备注 |
| --- | --- | --- | --- |
| body base | 512×512 | (1920, 1078) | UV `0.375,0 → 0.75,0.75` = 区域 A |
| body outline | 512×512 | (1920, 1078) | UV `0,0 → 0.375,0.75` = 区域 B |
| 左眼 | 204.8×204.8 | (1950.72, 1052.4) | UV `0.625,0.75 → 0.75,1` = G1 普通眼 |
| 右眼 | 204.8×204.8 | (2017.28, 1052.4) | 同左眼，`flip=true` 水平镜像 |
| 左脚 | 341.33×170.67 | (1864, 1190) | UV `0.75,0.5 → 1,0.75` = E 脚 base |
| 右脚 | 341.33×170.67 | (1976, 1190) | 同左脚 |
| 脚 outline | 341.33×170.67 | (1864/1976, 1190) | UV `0.75,0.25 → 1,0.5` = F 脚 outline |

- 共 **8 quads**（body/outline 各 1 + 左右眼 2 + 左右脚 base/outline 4）
- 眼睛在身体上方、脚在身体下方正常下垂，无重叠

---

## 6. 关键代码要点

### 6.1 眼睛（`src/tee_renderer.cpp`，skin6 双眼）

```cpp
const float EyeScale = pInfo->m_Skin6EyePair ? BaseSize * 0.40f : BaseSize * 0.60f;
const float h = pInfo->m_Skin6EyePair ? BaseSize * 0.40f : EyeScale; // 方形
const float EyeSeparation = (0.075f - 0.010f * Abs(Direction.x))
    * BaseSize * BodyScale.x * pInfo->m_Skin6EyeSeparationScale;
SubmitQuad(EyesTexture, BodyPos + Offset + vec2(-EyeSeparation, 0.0f), EyeScale * BodyScale.x, h, ..., U0, V0, U1, V1);
SubmitQuad(EyesTexture, BodyPos + Offset + vec2( EyeSeparation, 0.0f), EyeScale * BodyScale.x, h, ..., U0, V0, U1, V1, true); // 右眼镜像
```

### 6.2 行走动画必须叠加 base（`example/main.cpp`）

```cpp
CAnimState WalkState;
WalkState.Set(&s_aAnimations[ANIM_BASE], 0.0f);
WalkState.Add(&s_aAnimations[ANIM_WALK], 0.5f, 1.0f);
```

### 6.3 区域配置（4K 模板，`example/main.cpp`）

```cpp
const float TW = 4096.0f, TH = 2048.0f;
Renderer.SetSpriteRegion(TEE_SPRITE_BODY,         SSpriteRegion(0.0f/TW, 0.0f/TH, 1536.0f/TW, 1536.0f/TH, 1536, 1536)); // A
Renderer.SetSpriteRegion(TEE_SPRITE_BODY_OUTLINE, SSpriteRegion(1536.0f/TW, 0.0f/TH, 3072.0f/TW, 1536.0f/TH, 1536, 1536)); // B
Renderer.SetSpriteRegion(TEE_SPRITE_FOOT,         SSpriteRegion(3072.0f/TW, 512.0f/TH, 4096.0f/TW, 1024.0f/TH, 1024, 512)); // E
Renderer.SetSpriteRegion(TEE_SPRITE_FOOT_OUTLINE, SSpriteRegion(3072.0f/TW, 1024.0f/TH, 4096.0f/TW, 1536.0f/TH, 1024, 512)); // F
Renderer.SetSpriteRegion(TEE_SPRITE_EYES_NORMAL,  SSpriteRegion(1024.0f/TW, 1536.0f/TH, 1536.0f/TW, 2048.0f/TH, 512, 512)); // G1
// ... G2 angry / G3 pain / G4 happy / H surprise 同理
```

### 6.4 平滑缩放：Mipmap + 三线性过滤

原游戏缩放视角时 Tee 和表情仍保持平滑、无明显锯齿，核心在于 OpenGL3 后端的纹理创建策略（`src/engine/client/backend/opengl/backend_opengl3.cpp`）：

1. **独立 sprite 纹理**：皮肤每个部件（body/foot/eyes 等）和表情都通过 `LoadSpriteTexture` 从图集中裁剪成独立纹理，再各自生成 mipmap，避免整张图集 mipmap 导致 sprite 互相污染。
2. **自动生成 Mipmap**：默认 `Flags=0`（不设置 `TEXLOAD_NO_MIPMAPS`），后端调用 `glGenerateMipmap(GL_TEXTURE_2D)` 生成完整 mipmap 链。
3. **三线性过滤**：
   - `GL_TEXTURE_MIN_FILTER = GL_LINEAR_MIPMAP_LINEAR`
   - `GL_TEXTURE_MAG_FILTER = GL_LINEAR`
4. **LOD 限制**：对 ≥1024×1024 的纹理限制 `GL_TEXTURE_MAX_LEVEL/MAX_LOD = 5`，防止缩到极小时出现 mipmap 显示 bug。
5. **LOD BIAS**：支持 `m_OpenGLTextureLodBIAS` 微调 mipmap 选择。

**与本模块的关系**：`ITeeRenderBackend` 在创建纹理时携带 flags（如 `TEXLOAD_NO_MIPMAPS`），宿主可在 OpenGL/Vulkan/Metal/D3D 后端实现相同策略。HTML/Canvas 演示目前仅用 `drawImage` 双线性插值，无 mipmap，缩小时效果会差一些。

---

## 7. 眼部与脚部运动机制（QMClient 源码调查，2026-08-08）

### 7.1 眼部精灵：跟随鼠标/瞄准方向

**数据流**（`players.cpp` → `render.cpp`）：

1. `GetPlayerTargetAngle()`（`players.cpp`）返回瞄准角度：
   - **本地玩家**：`angle(m_Controls.m_aMousePos[dummy])` —— 直接用**鼠标位置**算角度
   - **其他玩家**：网络对象 `m_Angle`（服务器同步的瞄准角度，也是对方鼠标决定）或 `m_TargetX/Y`（扩展信息），再做 intra 插值
2. `Direction = direction(Angle)` —— 指向瞄准目标的单位向量，作为 `RenderTee(..., Direction, ...)` 的 `Dir` 参数
3. `render.cpp` 中眼睛 offset（眼睛在身体内滑动）：
   ```cpp
   vec2 Offset = vec2(Direction.x * 0.125f, -0.05f + Direction.y * 0.10f) * BaseSize;
   // skin6 双眼（render.cpp:619）再叠加 ±EyeSeparation：
   // 左眼 = BodyPos - EyeSeparation + Offset；右眼 = BodyPos + EyeSeparation + Offset（水平镜像）
   ```

**结论**：眼睛位置 = `BodyPos + Offset`，其中 `Offset` 完全由瞄准方向 `Direction`（= 鼠标方向）决定——眼睛会随鼠标在眼窝内滑动。

### 7.2 脚部精灵：固定动画轨迹

**数据流**（`players.cpp` 构造状态 → `render.cpp` 绘制）：

1. 状态构造（`players.cpp`）：
   ```cpp
   CAnimState State;
   State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0.0f);   // 先 base 姿态
   if(InAir)        State.Add(&ANIM_INAIR, 0.0f, 1.0f);
   else if(Stationary && Inactive) State.Add(SIT_LEFT/RIGHT, 0.0f, 1.0f);
   else if(Stationary) State.Add(&ANIM_IDLE, 0.0f, 1.0f);
   else if(!WantOtherDir) {
       if(Running) State.Add(RUN_LEFT/RIGHT, RunTime, 1.0f);
       else        State.Add(&ANIM_WALK, WalkTime, 1.0f);
   }
   // 武器动画叠加（hammer/ninja swing）
   ```
2. **轨迹相位由移动距离决定（不是时间）**：
   ```cpp
   float WalkTime = std::fmod(Position.x, 100.0f) / 100.0f;  // 每 100 单位一个步态周期
   float RunTime  = std::fmod(Position.x, 200.0f) / 200.0f;  // 每 200 单位一个周期
   ```
   → 脚部动画的相位跟着玩家 x 位移走，避免"太空步"，这是"固定运动轨迹"的关键
3. 脚部关键帧（`tee_anim.cpp`，来自 `content.py`）：`aWalkBackFoot` / `aWalkFrontFoot` 定义每帧 `(x, y, angle)` 抬脚-迈步-落地轨迹
4. 绘制（`render.cpp`）：前脚/后脚分别取关键帧，脚位 = `Position + pFoot*(X,Y) * AnimScale`，旋转 = `pFoot->m_Angle * 2π + FeetAngle`

**结论**：脚部位置/旋转完全由 `CAnimState` 的关键帧插值决定（固定轨迹），与鼠标无关；动画相位由角色移动距离（`Position.x`）驱动。

### 7.3 与本提取管线的对应

- 眼睛"跟随鼠标"：`CTeeRenderer::RenderTee` 已有 `Dir` 参数，眼睛 offset 公式已移植（`tee_renderer.cpp`）。宿主只需把瞄准方向（如鼠标方向）传入 `Dir` 即可复现。当前示例传固定 `vec2(1,0)`，所以眼睛固定朝右。
- 脚部"固定轨迹"：`CAnimState` + 关键帧已移植（`tee_anim.cpp`）。宿主需自己按 7.2 的距离公式算 `WalkTime/RunTime` 并 `Set(ANIM_BASE,0)` + `Add(...)` 构造状态。当前示例只渲染 idle，未演示 walk。

### 7.4 特殊眼睛：挂机睡觉 / 旁观 / 眨眼的 `EMOTE_BLINK`

挂机（AFK）、暂停、旁观以及周期性眨眼时用的"闭眼"眼睛 = `EMOTE_BLINK`。**皮肤文件里没有独立素材**，它就是把**默认 NORMAL 眼睛纹理压扁**渲染出来的。

**服务器端决定**（`src/game/server/entities/character.cpp` `DetermineEyeEmote()`）：
```cpp
if(GetPlayer()->IsAfk() || GetPlayer()->IsPaused())
    return IsFrozen ? EMOTE_NORMAL : EMOTE_BLINK;   // 挂机/暂停 → 眨眼闭眼
if(m_EmoteType != EMOTE_NORMAL)                     // /emote 手动设置优先
    return m_EmoteType;
if(IsFrozen)                                        // 深度冻结→痛,普通冻结→眨眼
    return (m_Core.m_DeepFrozen || m_Core.m_LiveFrozen) ? EMOTE_PAIN : EMOTE_BLINK;
if(HasNinjajetpack && ...) return EMOTE_HAPPY;
if(5 * TickSpeed() - ((Tick - m_LastAction) % (5 * TickSpeed())) < 5)
    return EMOTE_BLINK;                             // 每 5 秒周期自动眨眼
return EMOTE_NORMAL;
```

**传递链路**：`CCharacter::SnapCharacter()` 每 tick 调 `DetermineEyeEmote()` → 结果写入网络快照 `CNetObj_Character::m_Emote` → 客户端 `players.cpp` 读 `Player.m_Emote` 传给 `RenderTee(..., Player.m_Emote, ...)`。

**旁观 / 挂机的直接渲染**：
- 旁观：`players.cpp:1897` 强制 `RenderTee(GetIdle(), ..., EMOTE_BLINK, ...)`
- 服务器列表 AFK 头像：`menus_browser.cpp` `m_Afk ? EMOTE_BLINK : EMOTE_NORMAL`

**渲染实现（render.cpp，压扁 NORMAL 眼睛）**：
```cpp
// skin7（render.cpp:462）：高度压到 BaseSize*0.075（一条线）
float h = Emote == EMOTE_BLINK ? BaseSize * 0.15f / 2.0f : EyeScale / 2.0f;
// skin6（render.cpp:617）：高度压到 BaseSize*0.15
float h = (Emote == EMOTE_BLINK ? BaseSize * 0.15f : BaseSize * 0.40f) * BodyScale.y;
// 纹理：emote switch 无 BLINK 分支 → default 用 SPRITE_TEE_EYES_NORMAL；宽度保持正常 EyeScale
```

**提取管线已支持**（`tee_renderer.cpp:345-347` 已移植上述两个公式）：宿主把 `EMOTE_BLINK` 传给 `RenderTee` 即可渲染压扁闭眼，无需新增皮肤素材。

### 7.5 头顶表情（emoticon）渲染

头顶表情是**玩家层 + 引擎图集**的东西，与 Tee 本体皮肤无关（独立 `emoticons.png` 图集）。

**素材**（`datasrc/content.py`）：`image_emoticons = Image("emoticons","emoticons.png")`，`set_emoticons = SpriteSet(..., 4, 4)`，16 个精灵顺序 `SPRITE_OOP`(0,0)→`SPRITE_QUESTION`(3,3)：oop/exclamation/hearts/drop/dotdot/music/sorry/ghost/sushi/splattee/deviltee/zomg/zzz/wtf/eyes/question。客户端 `LoadEmoticonsSkin()` 按 `SPRITE_OOP+i` 加载到 `m_EmoticonsSkin.m_aSpriteEmoticons[16]`。

> 已用 `example/check_atlas.py` 验证实际 512×512 图集切分：cell_00=oop、cell_01=exclamation、cell_07=ghost、cell_15=question。

**发送**（`components/emoticon.cpp`）：
- `+emote` 命令 → 屏幕中央环形选择器（16 个表情绕圈，鼠标角度选中）→ `Emote(id)` 发 `CNetMsg_Cl_Emoticon`（`MSGFLAG_VITAL`）
- `EyeEmote(id)` 走 `/emote xxx N` 聊天命令（眼睛表情，不是头顶表情）
- 自动触发：被手雷炸到时随机发一个表情（`gameclient.cpp` `m_aLastRandomEmoteDamageTick`）

**接收**（`gameclient.cpp:2346`）：`NETMSGTYPE_SV_EMOTICON` → `m_aClients[id].m_Emoticon` + `m_EmoticonStartTick` + `m_EmoticonStartFraction`。

**头顶渲染**（`players.cpp:1226-1285`，玩家渲染末尾，绘制在 Tee 头顶）：
- 聊天中（`PLAYERFLAG_CHATTING`）→ 固定 `...`（SPRITE_DOTDOT）在 `Position+(24,-40)`
- 挂机（`Afk` && `m_ClAfkEmote`）→ 固定 `zzz`（SPRITE_ZZZ）在 `Position+(24,-40)`
- 普通表情（`m_ClShowEmotes` && !`m_EmoticonIgnore` && `m_EmoticonStartTick!=-1`）：
  - 时长 2 秒：`FromEnd = 2*TickSpeed - SinceStart`
  - 位置 `(Position.x, Position.y - 23 - 32*h)`（头顶上方），`h` 为入场缩放（前 0.1s 从 0→1）
  - 透明度 `a`：后 0.2s 从 1→0 淡出；`Wiggle=sin(5*t)` 左右摆动，旋转 `pi/6*WiggleAngle`
  - 用 `m_WeaponEmoteQuadContainerIndex` 预构建 quad 容器绘制

**缩放修复**：QMClient 的表情偏移/尺寸针对 64px Tee 编写。Tee 放大后必须乘 `Scale = TeeSize / 64`，否则表情散在四周。已在 `CEmoticonRenderer::SetTeeSize()` 实现：
- 普通表情位置：`TeePos + (0, (-23 - 32*h) * Scale)`
- 聊天/挂机位置：`TeePos + (24, -40) * Scale`
- 表情尺寸：`EmoticonSize * Scale (*h)`

**HTML 动态演示**：`example/emoticon_popup_demo.html` 用纯 Canvas 渲染完整 Tee（默认加载 `data/skins/ghostjtj.png` 256×128 protocol7 皮肤：身体/轮廓/双脚/双眼+鼠标跟随）+ 头顶表情；UV 区域已改为归一化坐标，可兼容不同尺寸的标准 protocol7 皮肤。实时播放 2 秒生命周期，冻结 0.03s/0.07s/0.12s 三帧展示 pop-in 过程；提供下拉选择器切换 16 个表情、Pause/Resume 按钮、Blink 复选框预览 `EMOTE_BLINK`、Walk 复选框直接复现 `tee_anim.cpp` 的真实 walk 关键帧动画。

**与本提取管线**：已提取为独立模块 `tee_emoticon.h/cpp`（`CEmoticonRenderer`），零依赖、走 `ITeeRenderBackend`；属于 Tee 渲染完成后的独立叠加层。迁移细节见 `EMOTICON_RENDER.md`。

---

## 8. 构建与运行

```bash
cd tee_render
cmake -S . -B build -G "MinGW Makefiles"   # 已配置
cmake --build build
./build/example/tee_render_example         # 生成 quads.txt + skin_render.svg
python example/render_skin.py              # 生成 skin_render.png（3840×2160）
```

> Windows 下从仓库根目录执行需先 `Push-Location tee_render`。

---

## 9. 未包含 / 后续可做

- **protocol 6（`RenderTee6`）**：未包含；如需旧版参考 `src/game/client/render.cpp`
- **皮肤加载/缓存**（`skins.cpp`）：属于引擎资源管理，与本管线正交
- **皮肤切换状态机**：提供纯函数 blend 计算，宿主自行管理进度
- **多 Tee 示例**：当前示例为单 Tee 放大渲染；如需之前的多姿态/多 Tee 对照可临时改回 `main.cpp`（历史版本见图 4.2 / 4.3 记录）
- **手部渲染**：protocol-7 的手部纹理（`SKINPART_HANDS` = 4K 模板 C/D 区域，`SPRITE_TEE_HAND`/`SPRITE_TEE_HAND_OUTLINE`）由玩家层 `CPlayers::RenderHand` 按瞄准方向动态绘制（用于握持武器/钩子），并不在 `RenderTee7` 里画。本提取管线未移植 `RenderHand`（属玩家层，依赖武器/瞄准），如需可参考 `players.cpp` 的 `RenderHand7` 补一个可选的 `RenderHand` 接口
- **表情 pop-in 动画**：已提取并验证；可继续扩展示例，例如把 16 个表情以时间轴方式全部展示

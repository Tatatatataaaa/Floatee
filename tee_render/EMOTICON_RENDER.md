# 表情渲染管线（Emoticon Render）提取文档

> 位置：`tee_render/`（与 `tee_render` 同目录的独立模块）
> 更新日期：2026-08-08
> 来源：QmClient `src/game/client/components/players.cpp`（`CPlayers::RenderPlayer` 末尾的表情绘制）

本文档记录把 QmClient 的"头顶表情"（emoticon）渲染管线提取为独立模块的过程，包括：素材、发送/接收、渲染算法、提取接口与示例。

---

## 1. 概述

头顶表情是显示在 **Tee 头顶上方**的气泡表情，与 Tee 皮肤**完全无关**：

- 独立的图集 `emoticons.png`（4×4 网格，16 个表情）
- 由玩家通过表情轮盘选择，或由游戏自动触发（挂机 `zzz`、聊天 `...`、被手雷炸到随机表情）
- 渲染时绘制在 Tee 头顶，带弹出/摆动/淡出动画

**为什么单独提取**：它属于玩家层/UI 层逻辑（`CPlayers::RenderPlayer`），与 Tee 本体渲染（`RenderTee7`）正交。提取为 `CEmoticonRenderer` 后，任何宿主都可在自己选定的位置（游戏中、记分板、菜单预览……）复现该效果。

---

## 2. 素材：emoticons.png

### 2.1 精灵定义（`datasrc/content.py`）

```python
image_emoticons = Image("emoticons", "emoticons.png")          # 512x512
set_emoticons   = SpriteSet("emoticons", image_emoticons, 4, 4) # 4x4 网格，每格 128px
```

16 个精灵按顺序（row-major，左上起）：

| 索引 | 名称 | 用途 | 索引 | 名称 | 用途 |
| --- | --- | --- | --- | --- | --- |
| 0 | oop | 普通 | 8 | sushi | 普通 |
| 1 | exclamation | 普通 | 9 | splattee | 普通 |
| 2 | hearts | 普通 | 10 | deviltee | 普通 |
| 3 | drop | 普通 | 11 | zomg | 普通 |
| 4 | **dotdot** | 聊天 `...` | 12 | **zzz** | 挂机睡觉 |
| 5 | music | 普通 | 13 | wtf | 普通 |
| 6 | sorry | 普通 | 14 | eyes | 普通 |
| 7 | ghost | 普通 | 15 | question | 普通 |

客户端 `LoadEmoticonsSkin()`（`gameclient.cpp:6757`）按 `SPRITE_OOP + i` 加载到 `m_EmoticonsSkin.m_aSpriteEmoticons[16]`。

**注意**：这里索引 i 是 row-major 顺序，但 QmClient 实际打包出来的 `emoticons.png` 也是按这个顺序排列的。可用 `example/check_atlas.py` 将 512×512 图切成 16 个 128×128 细胞验证，例如 `cell_07_ghost.png` 是幽灵、`cell_15_question.png` 是问号。HTML 演示 `example/emoticon_popup_demo.html` 中也是按此索引直接采样。

---

## 3. 发送 / 接收

### 3.1 发送（`components/emoticon.cpp`）

- `+emote` 命令 → 打开屏幕中央**环形选择器**（16 个表情绕中心圆排列，鼠标角度决定选中项）
- 选中后 `CEmoticon::Emote(id)` 发送 `CNetMsg_Cl_Emoticon`（`MSGFLAG_VITAL`）
- 自动触发：被手雷炸到时（`gameclient.cpp:4145-4166`）随机发一个表情

### 3.2 接收（`gameclient.cpp:2346`）

`NETMSGTYPE_SV_EMOTICON` 到达后记录三个字段：

```cpp
m_aClients[id].m_Emoticon           = pMsg->m_Emoticon;               // 0..15
m_aClients[id].m_EmoticonStartTick   = Client()->GameTick(Conn);       // 起始 tick
m_aClients[id].m_EmoticonStartFraction = Client()->IntraGameTickSincePrev(Conn);
```

> 提取模块不关心网络协议，只接收**表情索引 + 经过的秒数**即可。

---

## 4. 渲染算法（`players.cpp` 提取）

在 `CPlayers::RenderPlayer` 末尾，三种情况的绘制（表情 quad 基础尺寸 **64×64**，来自 `OnInit()` 的 `QuadContainerAddSprite(..., 64.f)`）：

### 4.1 聊天中 `...`（固定）

```cpp
// 条件：PLAYERFLAG_CHATTING && !Afk
// 表情：SPRITE_DOTDOT（索引 4）
// 位置：TeePos + (24, -40)，尺寸 64×64，无旋转
```

### 4.2 挂机 `zzz`（固定）

```cpp
// 条件：ClAfkEmote && Afk && 非自己的 dummy
// 表情：SPRITE_ZZZ（索引 12）
// 位置：TeePos + (24, -40)，尺寸 64×64，无旋转
```

### 4.3 普通表情（2 秒动画）

```cpp
// 条件：ClShowEmotes && !m_EmoticonIgnore && m_EmoticonStartTick != -1
float SinceStart = (GameTick - StartTick) + (IntraGameTickSincePrev - StartFraction); // 秒
float FromEnd    = (2 * TickSpeed) - SinceStart;   // 总时长 2 秒

if(0 <= SinceStart && FromEnd > 0)
{
    float a = 1;
    if(FromEnd < TickSpeed/5)  a = FromEnd / (TickSpeed/5);     // 后 0.2s 淡出

    float h = 1;
    if(SinceStart < TickSpeed/10) h = SinceStart / (TickSpeed/10); // 前 0.1s 弹出

    float Wiggle = 0;
    if(SinceStart < TickSpeed/5)  Wiggle = SinceStart / (TickSpeed/5);
    float WiggleAngle = sin(5 * Wiggle);                          // 摆动

    QuadsSetRotation(pi / 6 * WiggleAngle);                       // 旋转 ±30°
    // 位置：(TeePos.x, TeePos.y - 23 - 32*h)，尺寸 64*h × 64*h
    RenderQuadContainerAsSprite(..., TeePos.x, TeePos.y - 23.f - 32.f * h, h, h);
}
```

**逐时刻几何状态**（Tee 尺寸 64px，基准表情尺寸 64×64）：

| Elapsed | h（尺寸比） | 旋转 | 垂直偏移 | 透明度 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 0.00s | 0.0 | 0 | −23 | 1.0 | 尚未出现（尺寸为 0） |
| 0.03s | 0.3 | `sin(5·0.15)·π/6` ≈ 0.36 rad | −32.6 | 1.0 | 很小，轻微旋转 |
| 0.05s | 0.5 | `sin(5·0.25)·π/6` ≈ 0.52 rad | −39 | 1.0 | 半大，明显旋转 |
| 0.10s | 1.0 | `sin(5·0.5)·π/6` ≈ 0 | −55 | 1.0 | 弹出完成，旋转归零 |
| 0.20s | 1.0 | 0 | −55 | 1.0 | wiggle 结束，稳定 |
| 1.00s | 1.0 | 0 | −55 | 1.0 | 正常显示 |
| 1.90s | 1.0 | 0 | −55 | 1.0 | 即将淡出 |
| 2.00s | 1.0 | 0 | −55 | 0.0 | 结束 |

> 表中垂直偏移是相对于 `TeePos.y` 的 y 增量（向上为负）。表情 quad 以自身中心为锚点，所以 `y = TeePos.y + Offset`。最终表情底边大致在 `TeePos.y - 23`（刚出现）到 `TeePos.y - 55`（完全弹出）之间。

**动画参数汇总**（TickSpeed=50 换算为秒）：

| 参数 | 公式 | 值 |
| --- | --- | --- |
| 生命周期 | `2 * TickSpeed` | 2.0s |
| 弹出（pop-in） | `TickSpeed / 10` | 0.1s（h 0→1，位置从 y−23 上浮到 y−55） |
| 淡出（fade-out） | `TickSpeed / 5` | 0.2s（a 1→0） |
| 摆动（wiggle） | `TickSpeed / 5` | 0.2s（`sin(5t)` 旋转 ±30°） |

**视觉效果**：表情从 Tee 中心附近弹出并上浮到头顶（`y−23−32h`），同时左右摆动，最后淡出。

---

## 5. 提取接口（`tee_emoticon.h`）

```cpp
// 16 个表情索引
enum EEmoticonSprite { EMOTICON_OOP, ..., EMOTICON_DOTDOT, ..., EMOTICON_ZZZ, ..., NUM_EMOTICONS };

class CEmoticonRenderer
{
public:
    explicit CEmoticonRenderer(ITeeRenderBackend *pBackend, STextureHandle Texture);

    // 图集：emoticons.png 是 GridX x GridY 网格，前 NUM_EMOTICONS 格按行序映射
    void ConfigureEmoticonGrid(int GridX, int GridY);
    // 覆盖单个表情的 UV 区域（非网格图集用）
    void SetEmoticonRegion(EEmoticonSprite Sprite, const SSpriteRegion &Region);

    // 头顶普通表情（带弹出/摆动/淡出动画）
    void RenderEmoticon(const vec2 &TeePos, int Emoticon, float Elapsed, float Alpha);
    // 聊天中 "..."（固定，Tee 右上）
    void RenderChattingDots(const vec2 &TeePos, float Alpha);
    // 挂机 "zzz"（固定，Tee 右上）
    void RenderAfkZzz(const vec2 &TeePos, float Alpha);

    float m_EmoticonSize; // 表情 quad 基准尺寸（QMClient 用 64）
    float m_Scale;        // Tee 尺寸缩放（默认 1.0 = 64px Tee）
    void SetTeeSize(float TeeSize); // 按 Tee 尺寸缩放表情（TeeSize/64）
};
```

**与 `RenderTee7` 相同的架构**：零依赖、只向 `ITeeRenderBackend::DrawQuad` 提交一个纹理四边形（带 UV 区域）。表情纹理用 `STextureHandle` 抽象，宿主映射到真实的 `emoticons.png` 图集。

### 5.1 按 Tee 尺寸缩放（关键设计）

QMClient 的表情偏移/尺寸是针对**标准 64px Tee** 编写的（表情 64×64、头顶偏移 `-23/-32/+24/-40` 等）。宿主若把 Tee 渲染得更大（如本示例 256px），必须调用 `SetTeeSize(TeeSize)` 让表情同步缩放，否则表情会显得又小又偏、散在 Tee 四周（而不是头顶）：

```cpp
// 内部：m_Scale = TeeSize / 64
// 表情尺寸 = m_EmoticonSize * m_Scale（* h 动画缩放）
// 普通表情位置 = TeePos + (0, (-23 - 32*h) * m_Scale)
// 聊天/挂机位置 = TeePos + (24, -40) * m_Scale
```

效果：表情底边始终贴合在 Tee 头顶上方（`y - 23*m_Scale`），无论 Tee 多大。

**迁移到其他项目时只需复制以下公式**（`Scale = TeeSize / 64`）：

```cpp
const float Size  = EmoticonSize * Scale * h;
const vec2  Pos   = TeePos + vec2(0.0f, (-23.0f - 32.0f * h) * Scale);
const float Angle = (PI / 6.0f) * sin(5.0f * Wiggle);
// Alpha 与 h、Wiggle 定义同上
```

其中 `EmoticonSize` 可取 64（与 QmClient 一致），`TeePos` 是 Tee 渲染时传入的世界/屏幕坐标。若宿主没有 rotation API，可忽略 `Angle`（视觉差异主要是弹出阶段的小幅摆动）。

**平滑缩放**：表情 sprite 与原皮肤部件一样，上传时默认生成 mipmap 并使用 `GL_LINEAR_MIPMAP_LINEAR` / `GL_LINEAR` 三线性过滤。因此即使 Tee 被大幅缩小，表情仍然保持清晰无锯齿。宿主实现 `ITeeRenderBackend` 时应为表情纹理开启相同策略。

---

## 6. 示例（`example/main.cpp`）

渲染 4 个并排的 Tee（256px），每个头顶冻结在 pop-in 动画的不同阶段，纹理 id 2 = `emoticons.png`：

```cpp
CEmoticonRenderer EmoticonRenderer(&Backend, STextureHandle(2));
EmoticonRenderer.ConfigureEmoticonGrid(4, 4);
EmoticonRenderer.SetTeeSize(256.0f); // 表情随 Tee 缩放，贴合头顶

EmoticonRenderer.RenderEmoticon(vec2(aTeeX[0], TEE_Y), EMOTICON_OOP,         0.03f, 1.0f); // 刚出现
EmoticonRenderer.RenderEmoticon(vec2(aTeeX[1], TEE_Y), EMOTICON_EXCLAMATION, 0.07f, 1.0f); // 半弹出+摆动
EmoticonRenderer.RenderEmoticon(vec2(aTeeX[2], TEE_Y), EMOTICON_GHOST,       0.12f, 1.0f); // 基本弹出
EmoticonRenderer.RenderEmoticon(vec2(aTeeX[3], TEE_Y), EMOTICON_QUESTION,    1.0f,  1.0f); // 完全展开
```

**验证结果**（`quads.txt`，36 quads = 4×8 Tee + 4 表情；Tee 中心 `y=1110`，尺寸 256）：

| 纹理 | 位置 | 尺寸 | 旋转 | UV | 含义 |
| --- | --- | --- | --- | --- | --- |
| 2 | (576, ~980) | 76.8×76.8 | >0 | 0,0→0.25,0.25 | oop（0.03s，刚弹出） |
| 2 | (1470, ~928) | 179.2×179.2 | >0 | 0.25,0→0.5,0.25 | exclamation（0.07s，半弹出+摆动） |
| 2 | (2365, 890) | 256×256 | ≈0 | 0.75,0.25→1,0.5 | ghost（0.12s，基本弹出） |
| 2 | (3264, 890) | 256×256 | 0 | 0.75,0.75→1,1 | question（1.0s，完全展开） |

> 表情中心 `y≈890-980`，Tee 顶边 `y=982`——气泡正好悬在各自 Tee 头顶上方，不再散落四周。
>
> 注：前两个位置仍在上升过程中，所以 y 比后两个高（数值更小），尺寸也更小。

### 6.1 HTML 动态演示

`example/emoticon_popup_demo.html` 是一个零后端的纯前端示例，**直接复现了提取出的 Tee + 表情渲染管线**：

- 加载 `ghostjtj.png`（256×128 protocol7 皮肤图集），按归一化 UV 区域绘制，兼容不同尺寸的标准 protocol7 皮肤：
  - 身体（body）
  - 身体轮廓（body outline）
  - 双脚 base + outline
  - 双眼（带鼠标跟随：眼睛 offset 公式来自 `CRenderTools::RenderTee7`）
- 加载 `emoticons.png`，按 row-major UV 采样绘制头顶表情
- 前 3 个 Tee 冻结在 pop-in 的 0.03s / 0.07s / 0.12s
- 第 4 个 Tee 实时循环整个 2 秒生命周期
- 提供下拉选择器，可切换实时循环的表情（全部 16 个），观察不同美术资源的 pop-in 表现
- 提供 Pause / Resume 按钮
- 提供 Blink 复选框，预览挂机/睡眠/旁观用的 `EMOTE_BLINK`（将 NORMAL 眼睛纹理垂直压扁到 `BaseSize*0.15`）
- 提供 Walk 复选框，预览脚部 walk 动画（HTML 中直接复现 `tee_anim.cpp` 的真实 walk 关键帧数据：base + walk body/backFoot/frontFoot 线性插值叠加）

浏览器打开：`example/emoticon_popup_demo.html`（需与 `emoticons.png` 同目录；默认通过相对路径 `../../../data/skins/ghostjtj.png` 加载皮肤）。

`render_skin.py` 已更新为同时加载 `hollowknight.png`（id 1）和 `emoticons.png`（id 2）。

---

## 7. 与本管线其它部分的关系

| 部分 | 文件 | 说明 |
| --- | --- | --- |
| Tee 本体渲染 | `tee_renderer.h/cpp` | `RenderTee7`，皮肤/身体/眼/脚 |
| 动画 | `tee_anim.h/cpp` | `CAnimState` + 关键帧 |
| **头顶表情** | **`tee_emoticon.h/cpp`** | 本模块，独立于皮肤 |
| 后端抽象 | `tee_backend.h` | `ITeeRenderBackend`（表情与 Tee 共用） || 动态演示 | `example/emoticon_popup_demo.html` | 纯 HTML/Canvas 动画，展示 pop-in 全过程 |
表情是"在 Tee 渲染完成后叠加的一层"：宿主先 `Renderer.RenderTee(...)` 画 Tee，再 `EmoticonRenderer.RenderEmoticon(TeePos, ...)` 在头顶画表情。`TeePos` 即传给 `RenderTee` 的位置。

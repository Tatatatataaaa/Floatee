# 表情圆盘（Emoticon Wheel）实现记录

> 位置：`extracted/tee_render/`（记录文档，非提取模块）
> 更新日期：2026-08-10
> 来源：QmClient `src/game/client/components/emoticon.cpp` / `emoticon.h`（`CEmoticon`）

本文档记录 QmClient 的**表情圆盘（环形选择器）**实现，包括入口、几何布局、选择逻辑、渲染与动画，并评估哪些核心逻辑可以提取为零依赖纯函数。

---

## 1. 概述

表情圆盘是 `+emote` 命令打开的全屏中央环形选择器，用于快速选择：

- **外层环**：16 个**头顶表情**（emoticon，来自 `emoticons.png` 4×4 图集）
- **内层环**：6 个**眼睛表情**（eye emote，用 Tee 头像预览 `EMOTE_*`）

选择后：
- 外层表情 → 发送 `CNetMsg_Cl_Emoticon`（头顶弹出气泡）
- 内层眼睛 → 发送 `/emote <name> <duration>` 聊天命令（改变 Tee 眼睛）

与 Tee 本体渲染（`RenderTee7`）和头顶表情渲染（`CEmoticonRenderer`）不同，表情圆盘是**纯 UI 选择器**，属于 `CUi` 体系，不是游戏世界渲染管线的一部分。

---

## 2. 入口与生命周期

### 2.1 命令

```cpp
void CEmoticon::OnConsoleInit()
{
	Console()->Register("+emote", "", CFGFLAG_CLIENT, ConKeyEmoticon, this, "Open emote selector");
	Console()->Register("emote", "i[emote-id]", CFGFLAG_CLIENT, ConEmote, this, "Use emote");
}
```

- `+emote` 按下打开，松开关闭（`ConKeyEmoticon`）：`m_Active = pResult->GetInteger(0) != 0`
- 记分板打开时忽略；旁观/录像时忽略
- 与绑定轮盘（`CBindWheel`）互斥：绑轮盘激活时关掉表情轮盘

### 2.2 状态字段（`emoticon.h`）

```cpp
bool m_WasActive;              // 上一帧是否激活（用于关闭时发送选中项）
bool m_Active;                 // 当前是否激活
bool m_PresentationInitialized;
vec2 m_SelectorMouse;          // 鼠标相对屏幕中心的位置（累积 move）
int m_SelectedEmote;           // 当前悬停选中的头顶表情 (-1 = 无)
int m_SelectedEyeEmote;        // 当前悬停选中的眼睛表情 (-1 = 无)
CUi::CTouchState m_TouchState; // 触摸状态
bool m_TouchPressedOutside;    // 触摸点在圆外按下
```

### 2.3 关闭时的提交逻辑（`OnRender` 开头）

```cpp
if(!m_Active)
{
	if(m_TouchPressedOutside) { m_SelectedEmote = -1; m_SelectedEyeEmote = -1; m_TouchPressedOutside = false; }
	if(m_WasActive && m_SelectedEmote != -1)    Emote(m_SelectedEmote);
	if(m_WasActive && m_SelectedEyeEmote != -1) EyeEmote(m_SelectedEyeEmote);
	m_WasActive = false;
}
else
	m_WasActive = true;
```

> 关键：**选择不是"按下瞬间"触发，而是"松开时按最后悬停项提交"**。

---

## 3. 几何布局（核心参数）

`OnRender()` 内的常量（像素，相对屏幕中心）：

| 常量 | 值 | 含义 |
| --- | --- | --- |
| `s_InnerMouseLimitRadius` | 40.0f | 鼠标在此半径内 → 不选任何项 |
| `s_InnerOuterMouseBoundaryRadius` | 110.0f | 鼠标距离 > 此值 → 选外层（头顶表情）；40~110 → 选内层（眼睛） |
| `s_OuterMouseLimitRadius` | 170.0f | 鼠标拖出此半径会被钳制回 170 |
| `s_InnerItemRadius` | 70.0f | 内层眼睛 Tee 头像的分布半径 |
| `s_OuterItemRadius` | 150.0f | 外层表情 sprite 的分布半径 |
| `s_InnerCircleRadius` | 100.0f | 内层背景圆半径 |
| `s_OuterCircleRadius` | 190.0f | 外层背景圆半径 |

```
               ┌───────────────────────────┐
               │     外环背景 r=190         │
               │     16 个表情 r=150        │
               │   ┌─────────────────┐     │
               │   │ 分隔环 r=110    │     │
               │   │  6 个眼睛 r=70 │     │
               │   │  ┌─────────┐   │     │
               │   │  │ 中心 r=30│   │     │
               │   │  └─────────┘   │     │
               │   │ 内背景 r=100   │     │
               │   └─────────────────┘     │
               └───────────────────────────┘
```

---

## 4. 选择逻辑

### 4.1 鼠标输入

```cpp
bool CEmoticon::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;
	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);   // 累积相对屏幕中心
	return true;
}
```

触摸输入：按下位置距中心 ≤ 170 直接作为 `m_SelectorMouse`；> 190 记为"外部按下"。

### 4.2 每帧判定（`m_Active` 时）

```cpp
if(length(m_SelectorMouse) > s_OuterMouseLimitRadius)
	m_SelectorMouse = normalize(m_SelectorMouse) * s_OuterMouseLimitRadius; // 钳制

const float SelectorAngle = angle(m_SelectorMouse);   // atan2(y, x)

m_SelectedEmote = -1;
m_SelectedEyeEmote = -1;
if(length(m_SelectorMouse) > s_InnerOuterMouseBoundaryRadius)      // > 110 → 外层
	m_SelectedEmote = PositiveMod(std::round(SelectorAngle / (2π) * NUM_EMOTICONS), NUM_EMOTICONS); // 16 项
else if(length(m_SelectorMouse) > s_InnerMouseLimitRadius)          // 40~110 → 内层
	m_SelectedEyeEmote = PositiveMod(std::round(SelectorAngle / (2π) * NUM_EMOTES), NUM_EMOTES);   // 6 项
```

其中：

```cpp
static const auto PositiveMod = [](float x, float y) -> float {
	return std::fmod(x + y, y);   // 保证非负
};
```

> 核心公式：`Index = round(angle / (2π) * Count) mod Count`，即按鼠标角度均匀切分圆盘，四舍五入到最近扇区。

---

## 5. 渲染

### 5.1 背景（两个同心圆）

```cpp
Graphics()->BlendNormal();
Graphics()->TextureClear();
Graphics()->QuadsBegin();
Graphics()->SetColor(ui_token::color::SURFACE_OVERLAY.WithMultipliedAlpha(0.95f * PresentationAlpha));
Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, s_OuterCircleRadius * PresentationScale, 64);          // 外环 190
Graphics()->SetColor(ui_token::color::ACCENT_PRIMARY_DIM.WithMultipliedAlpha(0.95f * PresentationAlpha));
Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, s_InnerOuterMouseBoundaryRadius * PresentationScale, 64); // 分隔环 110
Graphics()->QuadsEnd();
```

### 5.2 外层：16 个头顶表情

```cpp
Graphics()->WrapClamp();
for(int Emote = 0; Emote < NUM_EMOTICONS; Emote++)   // 16
{
	float Angle = 2π * Emote / NUM_EMOTICONS;
	if(Angle > π) Angle -= 2π;                        // 归一到 [-π, π]

	Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[Emote]);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);           // 整张 sprite
	const float Reveal    = EmoticonStaggerReveal(Emote, NUM_EMOTICONS, PresentationAlpha);
	const float ItemAlpha = PresentationAlpha * Reveal;
	const float ItemScale = PresentationScale * (0.70f + 0.30f * Reveal);
	const vec2  Nudge     = direction(Angle) * s_OuterItemRadius * ItemScale;   // 150
	const float HoverPhase = Emote == m_SelectedEmote ? 1.0f : 0.0f;
	const float Size = (50.0f + HoverPhase * 30.0f) * ItemScale;                 // 悬停 50→80
	Graphics()->SetColor(1, 1, 1, ItemAlpha);
	IGraphics::CQuadItem QuadItem(ScreenCenter.x + Nudge.x, ScreenCenter.y + Nudge.y, Size, Size);
	Graphics()->QuadsDraw(&QuadItem, 1);
}
Graphics()->WrapNormal();
```

### 5.3 内层：6 个眼睛表情（Tee 头像预览）

```cpp
if(GameClient()->m_GameInfo.m_AllowEyeWheel && g_Config.m_ClEyeWheel && localTee存在)
{
	// 内层背景圆 r=100（SURFACE_HIGHLIGHT）
	CTeeRenderInfo TeeInfo = 本地 Tee 的 m_RenderInfo;
	for(int Emote = 0; Emote < NUM_EMOTES; Emote++)   // 6
	{
		float Angle = 2π * Emote / NUM_EMOTES;
		if(Angle > π) Angle -= 2π;
		const float Reveal    = EmoticonStaggerReveal(Emote, NUM_EMOTES, PresentationAlpha);
		const float ItemScale = PresentationScale * (0.76f + 0.24f * Reveal);
		const vec2  Nudge     = direction(Angle) * s_InnerItemRadius * ItemScale;   // 70
		const float HoverPhase = Emote == m_SelectedEyeEmote ? 1.0f : 0.0f;
		TeeInfo.m_Size = (48.0f + HoverPhase * 18.0f) * ItemScale;                   // 悬停 48→66
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, Emote, vec2(-1.0f, 0.0f), ScreenCenter + Nudge, ItemAlpha);
	}
	// 中心圆 r=30（SURFACE_ELEVATED）
}
```

> 内层用**真实 Tee 渲染**（idle 姿态 + 指定表情），尺寸 48px 悬停 66px，朝左（`Dir = (-1, 0)`）。

### 5.4 光标

```cpp
RenderTools()->RenderCursor(ScreenCenter + m_SelectorMouse * PresentationScale, 24.0f * PresentationScale, PresentationAlpha);
```

`RenderCursor`（`render.cpp:141`）：用 `IMAGE_CURSOR` 纹理画一个 `Size×Size` 的四边形（TL 对齐）。

---

## 6. 动画

### 6.1 弹簧呈现（spring presentation）

仅当 `m_QmExtraAnimations` 且 `UiRuntimeV2()` 启用时：

```cpp
static const float s_Spring. = { Stiffness=470, Damping=40, RestEpsilon=0.006, RestVelocity=0.08 };
PresentationAlpha = ResolveUiPresentationStateValue(ALPHA, active ? 1.0 : 0.0, Spring, 3, 0.004f); // 0~1
PresentationScale = ResolveUiPresentationStateValue(SCALE, active ? 1.0 : 0.88, Spring, 3, 0.004f);
```

- 打开：alpha 0→1、scale 0.88→1.0（轻微放大弹入）
- 关闭：alpha 1→0、scale 1.0→0.88（缩回）
- 非动画模式下：`PresentationAlpha = active ? 1 : 0`，`PresentationScale = active ? 1 : 0.88`

### 6.2 渐次 reveal（环形交错出现）

```cpp
static int EmoticonClockwiseOrderFromTop(int Index, int Count)
{
	const float Angle = (2π * Index) / Count;
	const float ClockwiseFromTop = std::fmod(Angle + π / 2.0f + 2π, 2π);
	return std::clamp((int)std::round(ClockwiseFromTop / (2π) * Count), 0, Count - 1);
}

static float EmoticonStaggerReveal(int Index, int Count, float PresentationAlpha)
{
	constexpr float MaxDelay = 0.42f;                       // 最晚项延迟 0.42s
	const int Order = EmoticonClockwiseOrderFromTop(Index, Count);
	const float Delay = MaxDelay * Order / (Count - 1);     // 从顶部顺时针交错
	const float LocalT = std::clamp((PresentationAlpha - Delay) / std::max(0.001f, 1.0f - Delay), 0.0f, 1.0f);
	const float Inv = 1.0f - LocalT;
	return 1.0f - Inv * Inv * Inv * Inv;                    // ease-out quartic
}
```

效果：圆盘打开时表情从**顶部顺时针**依次浮现（每个延迟 `0.42 * Order/(Count-1)`），单个用 ease-out quartic 淡入并放大（外层 0.70→1.0、内层 0.76→1.0 的 scale 系数）。

---

## 7. 发送机制

### 7.1 头顶表情 `Emote(int Emoticon)`

```cpp
void CEmoticon::Emote(int Emoticon)
{
	CNetMsg_Cl_Emoticon Msg;
	Msg.m_Emoticon = Emoticon;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
	// dummy copy moves：额外发一份给 dummy
}
```

### 7.2 眼睛表情 `EyeEmote(int Emote)`

```cpp
void CEmoticon::EyeEmote(int Emote)
{
	switch(Emote)
	{
	case EMOTE_NORMAL:   str_format(aBuf, "/emote normal %d",   g_Config.m_ClEyeDuration); break;
	case EMOTE_PAIN:     str_format(aBuf, "/emote pain %d",     g_Config.m_ClEyeDuration); break;
	case EMOTE_HAPPY:    str_format(aBuf, "/emote happy %d",    g_Config.m_ClEyeDuration); break;
	case EMOTE_SURPRISE: str_format(aBuf, "/emote surprise %d", g_Config.m_ClEyeDuration); break;
	case EMOTE_ANGRY:    str_format(aBuf, "/emote angry %d",    g_Config.m_ClEyeDuration); break;
	case EMOTE_BLINK:    str_format(aBuf, "/emote blink %d",    g_Config.m_ClEyeDuration); break;
	}
	GameClient()->m_Chat.SendChat(0, aBuf);   // 走聊天命令
}
```

> 眼睛表情走 `/emote` 聊天命令（服务端解析），时长由 `m_ClEyeDuration` 配置。

---

## 8. 常量汇总

| 名称 | 值 | 说明 |
| --- | --- | --- |
| `NUM_EMOTICONS` | 16 | 头顶表情数量（`emoticons.png` 4×4） |
| `NUM_EMOTES` | 6 | 眼睛表情数量（NORMAL/PAIN/HAPPY/SURPRISE/ANGRY/BLINK） |
| 外层 item 尺寸 | 50（悬停 +30） | 头顶表情 quad |
| 内层 item 尺寸 | 48（悬停 +18） | 眼睛 Tee 头像 |
| 光标尺寸 | 24 | `RenderCursor` |
| 中心圆 | 30 | `SURFACE_ELEVATED` |

---

## 9. 提取评估

### 9.1 可零依赖提取的纯逻辑

以下逻辑**不依赖引擎/UI**，可提取为纯函数：

1. **扇区选择**：`Index = PositiveMod(round(angle / (2π) * Count), Count)`
2. **环形布局**：第 i 项位置 = `Center + direction(2π * i / Count) * Radius`（角度归一到 `[-π, π]` 可选）
3. **渐次 reveal**：`EmoticonStaggerReveal(i, Count, t)`（从顶部顺时针交错 + ease-out quartic）
4. **弹簧呈现**：`PresentationAlpha/Scale` 的状态更新（可简化为宿主自行驱动）

这些可用在 HTML/Canvas 或任意宿主里复现圆盘的"布局 + 选择 + 动画"。

### 9.2 依赖引擎、需宿主提供的能力

| 能力 | 原实现 | 宿主需提供 |
| --- | --- | --- |
| 画圆 | `Graphics()->DrawCircle()` | 圆形填充原语（Canvas `arc` / GL 三角扇） |
| 画表情 sprite | `m_aSpriteEmoticons[i]` | 16 个表情纹理 + quad |
| 画 Tee 头像 | `RenderTools()->RenderTee()` | 复用 `tee_render` 的 `CTeeRenderer::RenderTee` |
| 画光标 | `RenderCursor()`（`IMAGE_CURSOR`） | 光标纹理 + 左上角对齐 quad |
| 颜色 | `ui_token::color::*` | 宿主配色方案 |

### 9.3 结论

- **头顶表情渲染**（pop-in 气泡）已提取为 `CEmoticonRenderer`（见 `EMOTICON_RENDER.md`）。
- **表情圆盘**是 UI 选择器，与 `CUi`（触摸、光标、颜色 token、spring）强耦合；完整提取 UI 渲染管线收益低。
- 建议：如需复现圆盘，**提取 §9.1 的纯逻辑 + 用宿主 UI 原语渲染**；或在 HTML/Canvas 中做一个交互 demo（复用 `emoticon_popup_demo.html` 的资源与 `tee_render` 布局）。

/*
 * tee_emoticon.cpp - Over-head emoticon renderer implementation.
 *
 * Faithful port of the emoticon drawing at the end of CPlayers::RenderPlayer
 * in QmClient's game/client/components/players.cpp:
 *   - "..." bubble while the player is chatting  (SPRITE_DOTDOT)
 *   - "zzz" bubble while the player is AFK       (SPRITE_ZZZ)
 *   - the 2-second pop-up emoticon (scale-in + wiggle + fade-out)
 *
 * Timings are converted from game ticks (TickSpeed = 50) to seconds:
 *   lifetime   2 * TickSpeed            -> 2.0s
 *   pop-in     TickSpeed / 10           -> 0.1s
 *   fade-out   TickSpeed / 5            -> 0.2s
 *   wiggle     TickSpeed / 5            -> 0.2s
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#include "tee_emoticon.h"

#include <cmath>

namespace teer
{

// ---- timings (converted from ticks) ---------------------------------------

static constexpr float EMOTICON_LIFETIME = 2.0f; // 2 * TickSpeed
static constexpr float EMOTICON_POP_IN = 0.1f; // TickSpeed / 10
static constexpr float EMOTICON_FADE_OUT = 0.2f; // TickSpeed / 5
static constexpr float EMOTICON_WIGGLE = 0.2f; // TickSpeed / 5

CEmoticonRenderer::CEmoticonRenderer(ITeeRenderBackend *pBackend, STextureHandle Texture) :
	m_pBackend(pBackend),
	m_Texture(Texture),
	m_EmoticonSize(64.0f),
	m_Scale(1.0f)
{
	// Default: every sprite samples the whole atlas until the host configures it.
}

void CEmoticonRenderer::ConfigureEmoticonGrid(int GridX, int GridY)
{
	if(GridX <= 0 || GridY <= 0)
		return;
	const float CellW = 1.0f / GridX;
	const float CellH = 1.0f / GridY;
	const int Count = GridX * GridY;
	for(int i = 0; i < Count && i < NUM_EMOTICONS; i++)
	{
		const int Col = i % GridX;
		const int Row = i / GridX;
		m_aRegions[i] = SSpriteRegion(Col * CellW, Row * CellH, (Col + 1) * CellW, (Row + 1) * CellH, 64.0f, 64.0f);
	}
}

void CEmoticonRenderer::SetEmoticonRegion(EEmoticonSprite Sprite, const SSpriteRegion &Region)
{
	if(Sprite >= 0 && Sprite < NUM_EMOTICONS)
		m_aRegions[Sprite] = Region;
}

void CEmoticonRenderer::RenderStatic(const vec2 &Pos, int Emoticon, float Alpha)
{
	if(m_pBackend == nullptr || Emoticon < 0 || Emoticon >= NUM_EMOTICONS)
		return;
	const SSpriteRegion &Region = m_aRegions[Emoticon];
	if(!Region.m_Valid)
		return;

	const float Size = m_EmoticonSize * m_Scale;
	STeeQuad Quad;
	Quad.m_Texture = m_Texture;
	Quad.m_Position = Pos;
	Quad.m_Width = Size;
	Quad.m_Height = Size;
	Quad.m_Rotation = 0.0f;
	Quad.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, Alpha);
	Quad.m_U0 = Region.m_U0;
	Quad.m_V0 = Region.m_V0;
	Quad.m_U1 = Region.m_U1;
	Quad.m_V1 = Region.m_V1;
	Quad.m_FlipX = false;
	m_pBackend->DrawQuad(Quad);
}

void CEmoticonRenderer::RenderChattingDots(const vec2 &TeePos, float Alpha)
{
	RenderStatic(TeePos + vec2(24.0f, -40.0f) * m_Scale, EMOTICON_DOTDOT, Alpha);
}

void CEmoticonRenderer::RenderAfkZzz(const vec2 &TeePos, float Alpha)
{
	RenderStatic(TeePos + vec2(24.0f, -40.0f) * m_Scale, EMOTICON_ZZZ, Alpha);
}

void CEmoticonRenderer::RenderEmoticon(const vec2 &TeePos, int Emoticon, float Elapsed, float Alpha)
{
	if(m_pBackend == nullptr || Emoticon < 0 || Emoticon >= NUM_EMOTICONS)
		return;
	const SSpriteRegion &Region = m_aRegions[Emoticon];
	if(!Region.m_Valid)
		return;

	// Only alive during the 2-second lifetime.
	if(Elapsed < 0.0f || Elapsed >= EMOTICON_LIFETIME)
		return;
	const float FromEnd = EMOTICON_LIFETIME - Elapsed;

	// Fade out over the last 0.2s.
	float a = 1.0f;
	if(FromEnd < EMOTICON_FADE_OUT)
		a = FromEnd / EMOTICON_FADE_OUT;

	// Pop in: scale h ramps 0->1 during the first 0.1s; the bubble also rises
	// from y - 23 toward y - 55 (position.y - 23 - 32*h).
	float h = 1.0f;
	if(Elapsed < EMOTICON_POP_IN)
		h = Elapsed / EMOTICON_POP_IN;

	// Wiggle sideways with sin(5 * t) during the first 0.2s.
	float Wiggle = 0.0f;
	if(Elapsed < EMOTICON_WIGGLE)
		Wiggle = Elapsed / EMOTICON_WIGGLE;
	const float WiggleAngle = std::sin(5.0f * Wiggle);

	const float Size = m_EmoticonSize * m_Scale * h;
	STeeQuad Quad;
	Quad.m_Texture = m_Texture;
	Quad.m_Position = TeePos + vec2(0.0f, (-23.0f - 32.0f * h) * m_Scale);
	Quad.m_Width = Size;
	Quad.m_Height = Size;
	Quad.m_Rotation = PI / 6.0f * WiggleAngle;
	Quad.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, a * Alpha);
	Quad.m_U0 = Region.m_U0;
	Quad.m_V0 = Region.m_V0;
	Quad.m_U1 = Region.m_U1;
	Quad.m_V1 = Region.m_V1;
	Quad.m_FlipX = false;
	m_pBackend->DrawQuad(Quad);
}

} // namespace teer

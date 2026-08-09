/*
 * tee_emoticon.h - Extracted "emoticon" (over-head face bubble) renderer.
 *
 * Port of the over-head emoticon drawing from QmClient's
 *   src/game/client/components/players.cpp  (the emoticon block at the end of
 *   RenderPlayer, drawing "..." while chatting, "zzz" while AFK, and the
 *   2-second pop-up emoticon the player picked from the wheel).
 *
 * Like the rest of the extracted pipeline this is engine-free: it only talks to
 * ITeeRenderBackend, so it can run on any graphics API. The emoticon artwork
 * lives in its own atlas (emoticons.png, a GridX x GridY grid, 16 sprites in
 * the classic order oop/exclamation/hearts/drop/dotdot/music/sorry/ghost/sushi/
 * splattee/deviltee/zomg/zzz/wtf/eyes/question), completely independent of the
 * tee skin.
 *
 * The renderer is a stateless computing layer: it takes the tee position, the
 * emoticon id and how long ago it started, and emits one textured quad through
 * the backend. The host decides when to call it (in-game, on the scoreboard,
 * in a menu preview, ...).
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#ifndef TEE_RENDER_TEE_EMOTICON_H
#define TEE_RENDER_TEE_EMOTICON_H

#include "tee_backend.h"
#include "tee_renderer.h" // SSpriteRegion

namespace teer
{

// Classic emoticon order (matches datasrc/content.py, SPRITE_OOP..SPRITE_QUESTION).
enum EEmoticonSprite
{
	EMOTICON_OOP = 0,
	EMOTICON_EXCLAMATION,
	EMOTICON_HEARTS,
	EMOTICON_DROP,
	EMOTICON_DOTDOT, // "..." while chatting
	EMOTICON_MUSIC,
	EMOTICON_SORRY,
	EMOTICON_GHOST,
	EMOTICON_SUSHI,
	EMOTICON_SPLATTEE,
	EMOTICON_DEVILTEE,
	EMOTICON_ZOMG,
	EMOTICON_ZZZ, // sleeping while AFK
	EMOTICON_WTF,
	EMOTICON_EYES,
	EMOTICON_QUESTION,
	NUM_EMOTICONS,
};

// Renders over-head emoticons for one tee. Each call emits at most one quad.
class CEmoticonRenderer
{
public:
	explicit CEmoticonRenderer(ITeeRenderBackend *pBackend, STextureHandle Texture);

	// ---- atlas setup -------------------------------------------------------
	// Configure the atlas as a GridX x GridY grid; the first NUM_EMOTICONS
	// cells (row-major, starting at the top-left) map to EMOTICON_*.
	void ConfigureEmoticonGrid(int GridX, int GridY);

	// Override a single sprite's UV region (for non-grid atlases).
	void SetEmoticonRegion(EEmoticonSprite Sprite, const SSpriteRegion &Region);

	// ---- drawing ------------------------------------------------------------
	// Pop-up emoticon floating above the tee's head.
	//   Emoticon : 0..NUM_EMOTICONS-1 (see EEmoticonSprite)
	//   Elapsed  : seconds since the emoticon was triggered (0..2s lifetime)
	//   Alpha    : global transparency multiplier (host-controlled)
	void RenderEmoticon(const vec2 &TeePos, int Emoticon, float Elapsed, float Alpha);

	// Fixed "..." bubble while the player is chatting (top-right of the tee).
	void RenderChattingDots(const vec2 &TeePos, float Alpha);

	// Fixed "zzz" bubble while the player is AFK / sleeping (top-right of the tee).
	void RenderAfkZzz(const vec2 &TeePos, float Alpha);

	// The base size of one emoticon quad (QMClient uses 64 for a size-64 tee).
	float m_EmoticonSize;

	// Scale for tees rendered larger than the stock 64px. QMClient's emoticon
	// offsets/sizes are authored for a 64px tee; when the host renders the tee
	// at m_Size, call SetTeeSize(m_Size) so the bubble stays glued to the head
	// (size and head offset both scale by m_Size / 64).
	float m_Scale;

	// Set the tee size so the emoticon scales/offsets match it (default 64).
	void SetTeeSize(float TeeSize) { m_Scale = TeeSize / 64.0f; }

private:
	ITeeRenderBackend *m_pBackend;
	STextureHandle m_Texture;
	SSpriteRegion m_aRegions[NUM_EMOTICONS];

	void RenderStatic(const vec2 &Pos, int Emoticon, float Alpha);
};

} // namespace teer

#endif // TEE_RENDER_TEE_EMOTICON_H

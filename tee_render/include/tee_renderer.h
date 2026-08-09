/*
 * tee_renderer.h - Core Tee renderer for the extracted pipeline.
 *
 * Port of CRenderTools::RenderTee / RenderTee7 and the size/bounds helpers
 * from DDNet's game/client/render.cpp, decoupled from IGraphics through
 * ITeeRenderBackend.
 *
 * The renderer is state-free apart from the sprite-region table, so a single
 * instance can be shared across the whole application.
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#ifndef TEE_RENDER_TEE_RENDERER_H
#define TEE_RENDER_TEE_RENDERER_H

#include "tee_anim.h"
#include "tee_backend.h"
#include "tee_math.h"
#include "tee_render_info.h"
#include "tee_skin.h"
#include "tee_types.h"

namespace teer
{

// UV region of a sprite inside its part texture. In the protocol-7 pipeline the
// part texture itself comes from the skin (Sixup.PartTexture / hat / bot), and
// the sprite only selects the UV region to sample (mirroring SelectSprite7).
// Sprites that are never SetSpriteRegion'd are considered invalid and skipped,
// so a renderer can enable only the parts it actually has textures for.
struct SSpriteRegion
{
	bool m_Valid;
	float m_U0, m_V0, m_U1, m_V1;
	float m_Width; // sprite pixel width (for scaling reference)
	float m_Height;

	SSpriteRegion() :
		m_Valid(false), m_U0(0.0f), m_V0(0.0f), m_U1(1.0f), m_V1(1.0f), m_Width(64.0f), m_Height(64.0f) {}
	SSpriteRegion(float U0, float V0, float U1, float V1, float Width, float Height) :
		m_Valid(true), m_U0(U0), m_V0(V0), m_U1(U1), m_V1(V1), m_Width(Width), m_Height(Height) {}
};

class CTeeRenderer
{
public:
	explicit CTeeRenderer(ITeeRenderBackend *pBackend);

	// ---- sizing helpers (ported from render.cpp) -----------------------------

	static void GetRenderTeeAnimScaleAndBaseSize(const STeeRenderInfo *pInfo, float &AnimScale, float &BaseSize);
	static void GetRenderTeeBodyScale(float BaseSize, float &BodyScale);
	static void GetRenderTeeFeetScale(float BaseSize, float &FeetScaleWidth, float &FeetScaleHeight);
	static void GetRenderTeeBodySize(const CAnimState *pAnim, const STeeRenderInfo *pInfo, vec2 &BodyOffset, float &Width, float &Height);
	static void GetRenderTeeFeetSize(const CAnimState *pAnim, const STeeRenderInfo *pInfo, vec2 &FeetOffset, float &Width, float &Height);

	// Bounding box (in pixels) of a rendered tee; useful for centering / layout.
	static void GetRenderTeeBounds(const CAnimState *pAnim, const STeeRenderInfo *pInfo, float AssumedScale, float &MinX, float &MinY, float &MaxX, float &MaxY);

	// Offset so that RenderTee draws exactly centered on Pos (ports GetRenderTeeOffsetToRenderedTee).
	static void GetRenderTeeOffsetToRenderedTee(const CAnimState *pAnim, const STeeRenderInfo *pInfo, vec2 &TeeOffsetToMid);

	// ---- main entry points -----------------------------------------------------

	// Render a tee at Pos with the given animation/emote/direction.
	// Direction is a unit vector pointing where the tee looks.
	void RenderTee(
		const CAnimState *pAnim,
		const STeeRenderInfo *pInfo,
		EEmote Emote,
		const vec2 &Dir,
		const vec2 &Pos,
		float Alpha = 1.0f,
		const vec2 &BodyScale = vec2(1.0f, 1.0f),
		const vec2 &FeetScale = vec2(1.0f, 1.0f),
		float BodyAngle = 0.0f,
		float FeetAngle = 0.0f) const;

	// Render with a skin-change transition between two appearances (QmClient feature).
	// Progress in [0,1]; TransitionType from ESkinChangeTransitionType.
	void RenderTeeWithSkinChangeTransition(
		const CAnimState *pAnim,
		const STeeRenderInfo *pPreviousInfo,
		const STeeRenderInfo *pCurrentInfo,
		EEmote Emote,
		const vec2 &Dir,
		const vec2 &Pos,
		float Progress,
		int TransitionType,
		float Alpha = 1.0f,
		const vec2 &BodyScale = vec2(1.0f, 1.0f),
		const vec2 &FeetScale = vec2(1.0f, 1.0f),
		float BodyAngle = 0.0f,
		float FeetAngle = 0.0f) const;

	// ---- sprite region table ---------------------------------------------------

	// Override the sprite region (UV + pixel size) for a Tee sprite.
	void SetSpriteRegion(ETeeSprite Sprite, const SSpriteRegion &Region);

	// Resolve the UV region for a sprite (ports SelectSprite7). Returns true when
	// a region is available (always true for the default full-texture region).
	bool ResolveSprite(ETeeSprite Sprite, float &U0, float &V0, float &U1, float &V1) const;

private:
	ITeeRenderBackend *m_pBackend;
	SSpriteRegion m_aSpriteRegions[NUM_TEE_SPRITES];

	void SubmitQuad(
		const STextureHandle &Texture,
		const vec2 &Center,
		float W,
		float H,
		float Rotation,
		const ColorRGBA &Color,
		float U0, float V0, float U1, float V1,
		bool FlipX = false) const;

	void RenderTee7(const CAnimState *pAnim, const STeeRenderInfo *pInfo, EEmote Emote, const vec2 &Dir, const vec2 &Pos, float Alpha, const vec2 &BodyScale, const vec2 &FeetScale, float BodyAngle, float FeetAngle) const;
};

} // namespace teer

#endif // TEE_RENDER_TEE_RENDERER_H

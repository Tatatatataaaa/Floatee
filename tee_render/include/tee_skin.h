/*
 * tee_skin.h - Skin data structures for the extracted Tee render pipeline.
 *
 * Port of DDNet's game/client/skin.h (CSkin::CSkinTextures) and the six-part
 * (protocol7) textures from CTeeRenderInfo::CSixup in game/client/render.h.
 *
 * IGraphics::CTextureHandle is replaced by an opaque uint32_t handle that the
 * host application assigns (e.g. an OpenGL texture id). The texture must be an
 * atlas containing the tee sprites in the layout described in README.md, or
 * the renderer's sprite-region table must be overridden.
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#ifndef TEE_RENDER_TEE_SKIN_H
#define TEE_RENDER_TEE_SKIN_H

#include <cstdint>

#include "tee_math.h"
#include "tee_types.h"

namespace teer
{

// Opaque texture handle, assigned by the host application.
struct STextureHandle
{
	uint32_t m_Id;

	STextureHandle() :
		m_Id(0) {}
	STextureHandle(uint32_t Id) :
		m_Id(Id) {}

	bool IsValid() const { return m_Id != 0; }
	void Invalidate() { m_Id = 0; }
};

// Protocol 6 skin textures (CSkin::CSkinTextures) - kept for the RenderTee6 path.
struct STeeSkinTextures
{
	STextureHandle m_Body;
	STextureHandle m_BodyOutline;
	STextureHandle m_Feet;
	STextureHandle m_FeetOutline;
	STextureHandle m_Hands;
	STextureHandle m_HandsOutline;
	STextureHandle m_aEyes[6];

	void Reset()
	{
		m_Body.Invalidate();
		m_BodyOutline.Invalidate();
		m_Feet.Invalidate();
		m_FeetOutline.Invalidate();
		m_Hands.Invalidate();
		m_HandsOutline.Invalidate();
		for(auto &Eye : m_aEyes)
			Eye.Invalidate();
	}
};

// Protocol 7 six-part skin. Each part is a dedicated texture (original and
// colorable variant), plus the optional hat / bot textures.
struct SSixupSkin
{
	STextureHandle m_aOriginalTextures[NUM_SKINPARTS];
	STextureHandle m_aColorableTextures[NUM_SKINPARTS];
	bool m_aUseCustomColors[NUM_SKINPARTS];
	ColorRGBA m_aColors[NUM_SKINPARTS];
	ColorRGBA m_BloodColor;
	STextureHandle m_HatTexture;
	STextureHandle m_BotTexture;
	int m_HatSpriteIndex; // 0..3, maps to TEE_SPRITE_HATS_*
	ColorRGBA m_BotColor;

	void Reset()
	{
		for(auto &T : m_aOriginalTextures)
			T.Invalidate();
		for(auto &T : m_aColorableTextures)
			T.Invalidate();
		for(auto &Use : m_aUseCustomColors)
			Use = false;
		for(auto &C : m_aColors)
			C = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_HatTexture.Invalidate();
		m_BotTexture.Invalidate();
		m_HatSpriteIndex = 0;
		m_BotColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
	}

	const STextureHandle &PartTexture(int Part) const
	{
		return (m_aUseCustomColors[Part] ? m_aColorableTextures : m_aOriginalTextures)[Part];
	}
};

} // namespace teer

#endif // TEE_RENDER_TEE_SKIN_H

/*
 * tee_render_info.h - Render info + flags for the extracted Tee render pipeline.
 *
 * Port of CTeeRenderInfo and the tee render flags from DDNet's game/client/render.h,
 * including the QmClient skin-change transition types and blend computation.
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#ifndef TEE_RENDER_TEE_RENDER_INFO_H
#define TEE_RENDER_TEE_RENDER_INFO_H

#include "tee_math.h"
#include "tee_skin.h"
#include "tee_types.h"

namespace teer
{

// ---- tee render flags (from game/client/render.h) ----------------------------
enum ETeeRenderFlag
{
	TEE_EFFECT_FROZEN = 1,
	TEE_NO_WEAPON = 2,
	TEE_EFFECT_SPARKLE = 4,
	TEE_CUSTOM_OUTLINE_COLOR = 8,
	TEE_PREVIEW_LAYER_BODY_OUTLINE = 1 << 8,
	TEE_PREVIEW_LAYER_BACK_FEET_OUTLINE = 1 << 9,
	TEE_PREVIEW_LAYER_FRONT_FEET_OUTLINE = 1 << 10,
	TEE_PREVIEW_LAYER_OUTLINE = TEE_PREVIEW_LAYER_BODY_OUTLINE | TEE_PREVIEW_LAYER_BACK_FEET_OUTLINE | TEE_PREVIEW_LAYER_FRONT_FEET_OUTLINE,
	TEE_PREVIEW_LAYER_BODY = 1 << 11,
	TEE_PREVIEW_LAYER_BACK_FEET = 1 << 12,
	TEE_PREVIEW_LAYER_FRONT_FEET = 1 << 13,
	TEE_PREVIEW_LAYER_FEET = TEE_PREVIEW_LAYER_BACK_FEET | TEE_PREVIEW_LAYER_FRONT_FEET,
	TEE_PREVIEW_LAYER_EYES = 1 << 14,
	TEE_PREVIEW_LAYER_ALL = TEE_PREVIEW_LAYER_OUTLINE | TEE_PREVIEW_LAYER_BODY | TEE_PREVIEW_LAYER_FEET | TEE_PREVIEW_LAYER_EYES,
};

inline int ResolveTeePreviewLayers(int TeeRenderFlags)
{
	const int PreviewLayers = TeeRenderFlags & TEE_PREVIEW_LAYER_ALL;
	return PreviewLayers != 0 ? PreviewLayers : TEE_PREVIEW_LAYER_ALL;
}

inline bool HasTeePreviewLayer(int TeeRenderFlags, int PreviewLayer)
{
	return (ResolveTeePreviewLayers(TeeRenderFlags) & PreviewLayer) != 0;
}

// ---- render info (CTeeRenderInfo port) ----------------------------------------
struct STeeRenderInfo
{
	STeeSkinTextures m_OriginalRenderSkin; // protocol 6 path
	STeeSkinTextures m_ColorableRenderSkin; // protocol 6 path

	bool m_CustomColoredSkin;
	ColorRGBA m_BloodColor;

	ColorRGBA m_ColorBody;
	ColorRGBA m_ColorFeet;
	ColorRGBA m_OutlineColor;
	float m_Size;
	bool m_GotAirJump;
	int m_TeeRenderFlags;
	bool m_FeetFlipped;
	// Skin6 eye regions contain one eye. When enabled, the renderer submits
	// two eyes and mirrors the second one, matching CRenderTools::RenderTee6.
	bool m_Skin6EyePair;
	// Multiplier for the source eye-center separation. Keep 1.0 for the exact
	// QmClient formula; increase it when a custom eye artwork has a wider
	// non-transparent silhouette than the stock skin6 artwork.
	float m_Skin6EyeSeparationScale;

	SSixupSkin m_aSixup[NUM_DUMMIES];

	STeeRenderInfo()
	{
		Reset();
	}

	void Reset()
	{
		m_OriginalRenderSkin.Reset();
		m_ColorableRenderSkin.Reset();
		m_CustomColoredSkin = false;
		m_BloodColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_OutlineColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		m_Size = 1.0f;
		m_GotAirJump = true;
		m_TeeRenderFlags = 0;
		m_FeetFlipped = false;
		m_Skin6EyePair = false;
		m_Skin6EyeSeparationScale = 1.0f;
		for(auto &Sixup : m_aSixup)
			Sixup.Reset();
	}

	bool Valid() const
	{
		// For the protocol 6 path the body texture determines validity; for the
		// protocol 7 path we check the body part of dummy 0.
		if(m_aSixup[0].m_aOriginalTextures[SKINPART_BODY].IsValid())
			return true;
		return m_CustomColoredSkin ? m_ColorableRenderSkin.m_Body.IsValid() : m_OriginalRenderSkin.m_Body.IsValid();
	}

	bool IsProtocol7() const { return m_aSixup[0].m_aOriginalTextures[SKINPART_BODY].IsValid(); }
};

// ---- skin change transition (QmClient feature) ---------------------------------
constexpr int DEFAULT_SKIN_CHANGE_TRANSITION_DURATION_MS = 500;

enum ESkinChangeTransitionType
{
	SKIN_CHANGE_TRANSITION_GHOST_POP = 0,
	SKIN_CHANGE_TRANSITION_FADE_SCALE,
	SKIN_CHANGE_TRANSITION_SLIDE_LEFT,
	SKIN_CHANGE_TRANSITION_SPIN_POP,
	SKIN_CHANGE_TRANSITION_THEME_SWITCH,
	SKIN_CHANGE_TRANSITION_TYPE_COUNT,
};

struct SSkinChangeTransitionBlend
{
	float m_PreviousAlpha = 0.0f;
	float m_CurrentAlpha = 1.0f;
	vec2 m_PreviousBodyScale = vec2(1.0f, 1.0f);
	vec2 m_PreviousFeetScale = vec2(1.0f, 1.0f);
	vec2 m_CurrentBodyScale = vec2(1.0f, 1.0f);
	vec2 m_CurrentFeetScale = vec2(1.0f, 1.0f);
	vec2 m_PreviousPosOffset = vec2(0.0f, 0.0f);
	vec2 m_CurrentPosOffset = vec2(0.0f, 0.0f);
	float m_PreviousAngleOffset = 0.0f;
	float m_CurrentAngleOffset = 0.0f;
};

inline float ClampSkinChangeTransitionProgress(float Progress)
{
	return Clamp(Progress, 0.0f, 1.0f);
}

inline float ResolveSkinChangeTransitionProgress(float ElapsedSeconds, int DurationMs)
{
	if(DurationMs <= 0)
		return 1.0f;
	const float DurationSeconds = Max(DurationMs, 0) / 1000.0f;
	if(DurationSeconds <= 0.0f)
		return 1.0f;
	return ClampSkinChangeTransitionProgress(ElapsedSeconds / DurationSeconds);
}

// Pure function computing the blend for the given transition type (port of
// ComputeSkinChangeTransitionBlend in render.h).
inline SSkinChangeTransitionBlend ComputeSkinChangeTransitionBlend(float Progress, const vec2 &BodyScale, const vec2 &FeetScale, int TransitionType)
{
	Progress = ClampSkinChangeTransitionProgress(Progress);
	TransitionType = Clamp(TransitionType, 0, SKIN_CHANGE_TRANSITION_TYPE_COUNT - 1);

	const float EaseOut = 1.0f - std::pow(1.0f - Progress, 3.0f);
	const float Enter = 1.0f - EaseOut;
	const float Pop = std::sin(Progress * PI);
	SSkinChangeTransitionBlend Blend;

	switch(TransitionType)
	{
	case SKIN_CHANGE_TRANSITION_FADE_SCALE:
	{
		const float PreviousScaleFactor = 1.0f - 0.06f * EaseOut;
		const float CurrentScaleFactor = 0.88f + 0.12f * EaseOut;
		Blend.m_PreviousAlpha = 1.0f - EaseOut;
		Blend.m_CurrentAlpha = EaseOut;
		Blend.m_PreviousBodyScale = BodyScale * PreviousScaleFactor;
		Blend.m_PreviousFeetScale = FeetScale * PreviousScaleFactor;
		Blend.m_CurrentBodyScale = BodyScale * CurrentScaleFactor;
		Blend.m_CurrentFeetScale = FeetScale * CurrentScaleFactor;
		break;
	}
	case SKIN_CHANGE_TRANSITION_SLIDE_LEFT:
	{
		const float PreviousScaleFactor = 1.0f - 0.03f * EaseOut;
		const float CurrentScaleFactor = 0.97f + 0.03f * EaseOut;
		Blend.m_PreviousAlpha = 1.0f - EaseOut;
		Blend.m_CurrentAlpha = EaseOut;
		Blend.m_PreviousBodyScale = BodyScale * PreviousScaleFactor;
		Blend.m_PreviousFeetScale = FeetScale * PreviousScaleFactor;
		Blend.m_CurrentBodyScale = BodyScale * CurrentScaleFactor;
		Blend.m_CurrentFeetScale = FeetScale * CurrentScaleFactor;
		Blend.m_PreviousPosOffset = vec2(-14.0f * EaseOut, 0.0f);
		Blend.m_CurrentPosOffset = vec2(18.0f * Enter, 0.0f);
		break;
	}
	case SKIN_CHANGE_TRANSITION_SPIN_POP:
	{
		const float PreviousScaleFactor = 1.0f - 0.04f * EaseOut;
		const float CurrentScaleFactor = 0.92f + 0.08f * EaseOut + 0.03f * Pop;
		Blend.m_PreviousAlpha = 1.0f - EaseOut;
		Blend.m_CurrentAlpha = EaseOut;
		Blend.m_PreviousBodyScale = BodyScale * PreviousScaleFactor;
		Blend.m_PreviousFeetScale = FeetScale * PreviousScaleFactor;
		Blend.m_CurrentBodyScale = BodyScale * CurrentScaleFactor;
		Blend.m_CurrentFeetScale = FeetScale * CurrentScaleFactor;
		Blend.m_PreviousAngleOffset = -0.18f * (1.0f - Progress);
		Blend.m_CurrentAngleOffset = 0.20f * Enter;
		break;
	}
	case SKIN_CHANGE_TRANSITION_THEME_SWITCH:
	{
		const float PreviousScaleFactor = 1.0f - 0.02f * EaseOut;
		const float CurrentScaleFactor = 0.96f + 0.04f * EaseOut;
		Blend.m_PreviousAlpha = 1.0f - EaseOut;
		Blend.m_CurrentAlpha = EaseOut;
		Blend.m_PreviousBodyScale = BodyScale * PreviousScaleFactor;
		Blend.m_PreviousFeetScale = FeetScale * PreviousScaleFactor;
		Blend.m_CurrentBodyScale = BodyScale * CurrentScaleFactor;
		Blend.m_CurrentFeetScale = FeetScale * CurrentScaleFactor;
		Blend.m_PreviousPosOffset = vec2(0.0f, -8.0f * EaseOut);
		Blend.m_CurrentPosOffset = vec2(0.0f, 8.0f * Enter);
		break;
	}
	case SKIN_CHANGE_TRANSITION_GHOST_POP:
	default:
	{
		const float PreviousScaleFactor = 1.0f - 0.06f * EaseOut;
		const float CurrentScaleFactor = 0.94f + 0.06f * EaseOut + 0.05f * Pop;
		Blend.m_PreviousAlpha = 1.0f - EaseOut;
		Blend.m_CurrentAlpha = 0.18f + 0.82f * EaseOut;
		Blend.m_PreviousBodyScale = BodyScale * PreviousScaleFactor;
		Blend.m_PreviousFeetScale = FeetScale * PreviousScaleFactor;
		Blend.m_CurrentBodyScale = BodyScale * CurrentScaleFactor;
		Blend.m_CurrentFeetScale = FeetScale * CurrentScaleFactor;
		break;
	}
	}

	return Blend;
}

} // namespace teer

#endif // TEE_RENDER_TEE_RENDER_INFO_H

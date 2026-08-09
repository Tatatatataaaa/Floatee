/*
 * tee_renderer.cpp - Core Tee renderer implementation.
 *
 * Faithful port of CRenderTools::RenderTee / RenderTee7 from DDNet's
 * game/client/render.cpp, re-targeted onto ITeeRenderBackend.
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#include "tee_renderer.h"

#include <cmath>

namespace teer
{

CTeeRenderer::CTeeRenderer(ITeeRenderBackend *pBackend) :
	m_pBackend(pBackend)
{
	// Default: full-texture region for every sprite, i.e. each skin part texture
	// is sampled in full. The host can override regions for atlas layouts.
}

void CTeeRenderer::SetSpriteRegion(ETeeSprite Sprite, const SSpriteRegion &Region)
{
	if(Sprite >= 0 && Sprite < NUM_TEE_SPRITES)
		m_aSpriteRegions[Sprite] = Region;
}

bool CTeeRenderer::ResolveSprite(ETeeSprite Sprite, float &U0, float &V0, float &U1, float &V1) const
{
	if(Sprite < 0 || Sprite >= NUM_TEE_SPRITES)
		return false;
	const SSpriteRegion &Region = m_aSpriteRegions[Sprite];
	if(!Region.m_Valid)
		return false;
	U0 = Region.m_U0;
	V0 = Region.m_V0;
	U1 = Region.m_U1;
	V1 = Region.m_V1;
	return true;
}

void CTeeRenderer::SubmitQuad(
	const STextureHandle &Texture,
	const vec2 &Center,
	float W,
	float H,
	float Rotation,
	const ColorRGBA &Color,
	float U0, float V0, float U1, float V1,
	bool FlipX) const
{
	if(!Texture.IsValid() || m_pBackend == nullptr)
		return;

	STeeQuad Quad;
	Quad.m_Texture = Texture;
	Quad.m_Position = Center;
	Quad.m_Width = W;
	Quad.m_Height = H;
	Quad.m_Rotation = Rotation;
	Quad.m_Color = Color;
	Quad.m_U0 = U0;
	Quad.m_V0 = V0;
	Quad.m_U1 = U1;
	Quad.m_V1 = V1;
	Quad.m_FlipX = FlipX;
	m_pBackend->DrawQuad(Quad);
}

// ---------------------------------------------------------------------------
// Sizing helpers (ported from render.cpp)
// ---------------------------------------------------------------------------

void CTeeRenderer::GetRenderTeeAnimScaleAndBaseSize(const STeeRenderInfo *pInfo, float &AnimScale, float &BaseSize)
{
	AnimScale = pInfo->m_Size * 1.0f / 64.0f;
	BaseSize = pInfo->m_Size;
}

void CTeeRenderer::GetRenderTeeBodyScale(float BaseSize, float &BodyScale)
{
	BodyScale = BaseSize; // cl_fat_skins not carried over: keep 1:1
	BodyScale /= 64.0f;
}

void CTeeRenderer::GetRenderTeeFeetScale(float BaseSize, float &FeetScaleWidth, float &FeetScaleHeight)
{
	FeetScaleWidth = BaseSize / 64.0f;
	FeetScaleHeight = (BaseSize / 2) / 32.0f;
}

void CTeeRenderer::GetRenderTeeBodySize(const CAnimState *pAnim, const STeeRenderInfo *pInfo, vec2 &BodyOffset, float &Width, float &Height)
{
	(void)pAnim;
	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);

	float BodyScale;
	GetRenderTeeBodyScale(BaseSize, BodyScale);

	// Without skin metrics (extracted pipeline uses a fixed 64x64 body), we use
	// the classic DDNet 64x64 body quad.
	Width = 64.0f * BodyScale;
	Height = 64.0f * BodyScale;
	BodyOffset = vec2(0.0f, 0.0f);
}

void CTeeRenderer::GetRenderTeeFeetSize(const CAnimState *pAnim, const STeeRenderInfo *pInfo, vec2 &FeetOffset, float &Width, float &Height)
{
	(void)pAnim;
	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);

	float FeetScaleWidth, FeetScaleHeight;
	GetRenderTeeFeetScale(BaseSize, FeetScaleWidth, FeetScaleHeight);

	Width = 64.0f * FeetScaleWidth;
	Height = 32.0f * FeetScaleHeight;
	FeetOffset = vec2(0.0f, 0.0f);
}

void CTeeRenderer::GetRenderTeeBounds(const CAnimState *pAnim, const STeeRenderInfo *pInfo, float AssumedScale, float &MinX, float &MinY, float &MaxX, float &MaxY)
{
	const vec2 BodyPos = vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AssumedScale;
	vec2 BodyOffset;
	float BodyWidth, BodyHeight;
	GetRenderTeeBodySize(pAnim, pInfo, BodyOffset, BodyWidth, BodyHeight);
	MinX = -32.0f * AssumedScale + BodyPos.x + BodyOffset.x;
	MinY = -32.0f * AssumedScale + BodyPos.y + BodyOffset.y;
	MaxX = MinX + BodyWidth;
	MaxY = MinY + BodyHeight;

	// Expand with feet.
	vec2 FeetOffset;
	float FeetWidth, FeetHeight;
	GetRenderTeeFeetSize(pAnim, pInfo, FeetOffset, FeetWidth, FeetHeight);
	const vec2 aFeetPos[2] = {
		vec2(pAnim->GetFrontFoot()->m_X, pAnim->GetFrontFoot()->m_Y) * AssumedScale,
		vec2(pAnim->GetBackFoot()->m_X, pAnim->GetBackFoot()->m_Y) * AssumedScale,
	};
	for(const vec2 &FootPos : aFeetPos)
	{
		const float FootMinX = -32.0f * AssumedScale + FootPos.x + FeetOffset.x;
		MinX = Min(MinX, FootMinX);
		MaxX = Max(MaxX, FootMinX + FeetWidth);
		MaxY = Max(MaxY, -16.0f * AssumedScale + FootPos.y + FeetOffset.y + FeetHeight);
	}
}

void CTeeRenderer::GetRenderTeeOffsetToRenderedTee(const CAnimState *pAnim, const STeeRenderInfo *pInfo, vec2 &TeeOffsetToMid)
{
	if(pInfo->m_aSixup[0].m_aOriginalTextures[SKINPART_BODY].IsValid())
	{
		TeeOffsetToMid = vec2(0.0f, pInfo->m_Size * 0.12f);
		return;
	}

	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);
	const float AssumedScale = BaseSize / 64.0f;
	float MinX, MinY, MaxX, MaxY;
	GetRenderTeeBounds(pAnim, pInfo, AssumedScale, MinX, MinY, MaxX, MaxY);
	TeeOffsetToMid.x = 0.0f;
	TeeOffsetToMid.y = -(MinY + (MaxY - MinY) / 2.0f);
}

// ---------------------------------------------------------------------------
// Main entry
// ---------------------------------------------------------------------------

void CTeeRenderer::RenderTee(
	const CAnimState *pAnim,
	const STeeRenderInfo *pInfo,
	EEmote Emote,
	const vec2 &Dir,
	const vec2 &Pos,
	float Alpha,
	const vec2 &BodyScale,
	const vec2 &FeetScale,
	float BodyAngle,
	float FeetAngle) const
{
	if(pInfo->m_aSixup[0].m_aOriginalTextures[SKINPART_BODY].IsValid())
		RenderTee7(pAnim, pInfo, Emote, Dir, Pos, Alpha, BodyScale, FeetScale, BodyAngle, FeetAngle);

	// Note: the protocol 6 (RenderTee6) path is intentionally not carried over;
	// the extracted pipeline targets the modern protocol-7 six-part skin renderer.
}

void CTeeRenderer::RenderTeeWithSkinChangeTransition(
	const CAnimState *pAnim,
	const STeeRenderInfo *pPreviousInfo,
	const STeeRenderInfo *pCurrentInfo,
	EEmote Emote,
	const vec2 &Dir,
	const vec2 &Pos,
	float Progress,
	int TransitionType,
	float Alpha,
	const vec2 &BodyScale,
	const vec2 &FeetScale,
	float BodyAngle,
	float FeetAngle) const
{
	if(pCurrentInfo == nullptr)
		return;

	Progress = ClampSkinChangeTransitionProgress(Progress);
	if(pPreviousInfo == nullptr || !pPreviousInfo->Valid() || Progress >= 1.0f)
	{
		RenderTee(pAnim, pCurrentInfo, Emote, Dir, Pos, Alpha, BodyScale, FeetScale, BodyAngle, FeetAngle);
		return;
	}

	const SSkinChangeTransitionBlend Blend = ComputeSkinChangeTransitionBlend(Progress, BodyScale, FeetScale, TransitionType);
	if(Blend.m_PreviousAlpha > 0.0f)
	{
		RenderTee(pAnim, pPreviousInfo, Emote, Dir, Pos + Blend.m_PreviousPosOffset, Alpha * Blend.m_PreviousAlpha, Blend.m_PreviousBodyScale, Blend.m_PreviousFeetScale, BodyAngle + Blend.m_PreviousAngleOffset, FeetAngle + Blend.m_PreviousAngleOffset);
	}
	if(Blend.m_CurrentAlpha > 0.0f)
	{
		RenderTee(pAnim, pCurrentInfo, Emote, Dir, Pos + Blend.m_CurrentPosOffset, Alpha * Blend.m_CurrentAlpha, Blend.m_CurrentBodyScale, Blend.m_CurrentFeetScale, BodyAngle + Blend.m_CurrentAngleOffset, FeetAngle + Blend.m_CurrentAngleOffset);
	}
}

// ---------------------------------------------------------------------------
// RenderTee7 (ported from render.cpp, protocol-7 six-part skin)
// ---------------------------------------------------------------------------

void CTeeRenderer::RenderTee7(
	const CAnimState *pAnim,
	const STeeRenderInfo *pInfo,
	EEmote Emote,
	const vec2 &Dir,
	const vec2 &Pos,
	float Alpha,
	const vec2 &BodyScale,
	const vec2 &FeetScale,
	float BodyAngle,
	float FeetAngle) const
{
	const vec2 Direction = Dir;
	const vec2 Position = Pos;
	const SSixupSkin &Sixup = pInfo->m_aSixup[0];
	const bool IsBot = Sixup.m_BotTexture.IsValid();

	if(m_pBackend != nullptr)
		m_pBackend->BeginTee();

	// first pass we draw the outline, second pass we draw the filling
	for(int Pass = 0; Pass < 2; Pass++)
	{
		const bool OutLine = Pass == 0;
		if(OutLine && !HasTeePreviewLayer(pInfo->m_TeeRenderFlags, TEE_PREVIEW_LAYER_OUTLINE))
			continue;

		for(int Filling = 0; Filling < 2; Filling++)
		{
			const float AnimScale = pInfo->m_Size * 1.0f / 64.0f;
			const float BaseSize = pInfo->m_Size;
			if(Filling == 1)
			{
				const vec2 BodyPos = Position + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
				const float BodyW = BaseSize * BodyScale.x;
				const float BodyH = BaseSize * BodyScale.y;
				const bool DrawBody = HasTeePreviewLayer(pInfo->m_TeeRenderFlags, OutLine ? TEE_PREVIEW_LAYER_BODY_OUTLINE : TEE_PREVIEW_LAYER_BODY);
				const bool DrawEyes = !OutLine && HasTeePreviewLayer(pInfo->m_TeeRenderFlags, TEE_PREVIEW_LAYER_EYES);

				float U0, V0, U1, V1;

				// ---- bot visuals (background / foreground / glow) ----
				if(DrawBody && IsBot && !OutLine)
				{
					const vec2 BotPos(BodyPos.x + (2.f / 3.f) * AnimScale, BodyPos.y + (-16 + 2.f / 3.f) * AnimScale);
					if(ResolveSprite(TEE_SPRITE_BOT_BACKGROUND, U0, V0, U1, V1))
						SubmitQuad(Sixup.m_BotTexture, BotPos, BodyW, BodyH, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha), U0, V0, U1, V1);
					if(ResolveSprite(TEE_SPRITE_BOT_FOREGROUND, U0, V0, U1, V1))
						SubmitQuad(Sixup.m_BotTexture, BotPos, BodyW, BodyH, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha), U0, V0, U1, V1);
					if(ResolveSprite(TEE_SPRITE_BOT_GLOW, U0, V0, U1, V1))
						SubmitQuad(Sixup.m_BotTexture, BotPos, BodyW, BodyH, 0.0f, Sixup.m_BotColor.WithAlpha(Alpha), U0, V0, U1, V1);
				}

				// ---- decoration ----
				if(DrawBody && ResolveSprite(OutLine ? TEE_SPRITE_DECORATION_OUTLINE : TEE_SPRITE_DECORATION, U0, V0, U1, V1))
				{
					const ColorRGBA DecorationColor = Sixup.m_aColors[SKINPART_DECORATION];
					SubmitQuad(Sixup.PartTexture(SKINPART_DECORATION), BodyPos, BodyW, BodyH, pAnim->GetBody()->m_Angle * PI * 2 + BodyAngle, DecorationColor.WithAlpha(Alpha), U0, V0, U1, V1);
				}

				// ---- body (behind marking) ----
				const STextureHandle &BodyTexture = Sixup.PartTexture(SKINPART_BODY);
				if(DrawBody && BodyTexture.IsValid())
				{
					const ColorRGBA BodyColor = OutLine ? ColorRGBA(1.0f, 1.0f, 1.0f, Alpha) : Sixup.m_aColors[SKINPART_BODY].WithAlpha(Alpha);
					if(ResolveSprite(OutLine ? TEE_SPRITE_BODY_OUTLINE : TEE_SPRITE_BODY, U0, V0, U1, V1))
					{
						SubmitQuad(BodyTexture, BodyPos, BodyW, BodyH, pAnim->GetBody()->m_Angle * PI * 2 + BodyAngle, BodyColor, U0, V0, U1, V1);
					}
				}

				// ---- marking ----
				if(DrawBody && !OutLine && ResolveSprite(TEE_SPRITE_MARKING, U0, V0, U1, V1))
				{
					const ColorRGBA MarkingColor = Sixup.m_aColors[SKINPART_MARKING];
					SubmitQuad(Sixup.PartTexture(SKINPART_MARKING), BodyPos, BodyW, BodyH, pAnim->GetBody()->m_Angle * PI * 2 + BodyAngle, ColorRGBA(MarkingColor.r * MarkingColor.a, MarkingColor.g * MarkingColor.a, MarkingColor.b * MarkingColor.a, MarkingColor.a * Alpha), U0, V0, U1, V1);
				}

				// ---- body shadow + upper outline (in front of marking) ----
				if(DrawBody && !OutLine)
				{
					if(ResolveSprite(TEE_SPRITE_BODY_SHADOW, U0, V0, U1, V1))
						SubmitQuad(BodyTexture, BodyPos, BodyW, BodyH, pAnim->GetBody()->m_Angle * PI * 2 + BodyAngle, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha), U0, V0, U1, V1);
					if(ResolveSprite(TEE_SPRITE_BODY_UPPER_OUTLINE, U0, V0, U1, V1))
						SubmitQuad(BodyTexture, BodyPos, BodyW, BodyH, pAnim->GetBody()->m_Angle * PI * 2 + BodyAngle, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha), U0, V0, U1, V1);
				}

				// ---- eyes ----
				if(DrawEyes)
				{
					const STextureHandle &EyesTexture = Sixup.PartTexture(SKINPART_EYES);
					if(EyesTexture.IsValid())
					{
						ETeeSprite EyeSprite = TEE_SPRITE_EYES_NORMAL;
						if(IsBot)
						{
							EyeSprite = TEE_SPRITE_EYES_SURPRISE;
							Emote = EMOTE_SURPRISE;
						}
						else
						{
							switch(Emote)
							{
							case EMOTE_PAIN: EyeSprite = TEE_SPRITE_EYES_PAIN; break;
							case EMOTE_HAPPY: EyeSprite = TEE_SPRITE_EYES_HAPPY; break;
							case EMOTE_SURPRISE: EyeSprite = TEE_SPRITE_EYES_SURPRISE; break;
							case EMOTE_ANGRY: EyeSprite = TEE_SPRITE_EYES_ANGRY; break;
							default: EyeSprite = TEE_SPRITE_EYES_NORMAL; break;
							}
						}

						if(ResolveSprite(EyeSprite, U0, V0, U1, V1))
						{
							const float EyeScale = pInfo->m_Skin6EyePair ? BaseSize * 0.40f : BaseSize * 0.60f;
							const float h = pInfo->m_Skin6EyePair ?
								(Emote == EMOTE_BLINK ? BaseSize * 0.15f : BaseSize * 0.40f) * BodyScale.y :
								(Emote == EMOTE_BLINK ? BaseSize * 0.15f / 2.0f : EyeScale);
							const vec2 Offset = vec2(Direction.x * 0.125f * pInfo->m_Skin6EyeOffsetScale, -0.05f + Direction.y * 0.10f * pInfo->m_Skin6EyeOffsetScale) * BaseSize;
							const ColorRGBA EyeColor = IsBot ? Sixup.m_BotColor.WithAlpha(Alpha) : Sixup.m_aColors[SKINPART_EYES].WithAlpha(Alpha);
							if(pInfo->m_Skin6EyePair)
							{
								const float EyeSeparation = (0.075f - 0.010f * Abs(Direction.x) * pInfo->m_Skin6EyeSeparationDirectionScale) * BaseSize * BodyScale.x * pInfo->m_Skin6EyeSeparationScale;
								SubmitQuad(EyesTexture, BodyPos + Offset + vec2(-EyeSeparation, 0.0f), EyeScale * BodyScale.x, h, pAnim->GetBody()->m_Angle * PI * 2 + BodyAngle, EyeColor, U0, V0, U1, V1);
								SubmitQuad(EyesTexture, BodyPos + Offset + vec2(EyeSeparation, 0.0f), EyeScale * BodyScale.x, h, pAnim->GetBody()->m_Angle * PI * 2 + BodyAngle, EyeColor, U0, V0, U1, V1, true);
							}
							else
							{
								SubmitQuad(EyesTexture, BodyPos + Offset, EyeScale, h, pAnim->GetBody()->m_Angle * PI * 2 + BodyAngle, EyeColor, U0, V0, U1, V1);
							}
						}
					}
				}

				// ---- xmas hat ----
				if(DrawBody && !OutLine && Sixup.m_HatTexture.IsValid())
				{
					ETeeSprite HatSprite = TEE_SPRITE_HATS_TOP1;
					switch(Sixup.m_HatSpriteIndex)
					{
					case 0: HatSprite = TEE_SPRITE_HATS_TOP1; break;
					case 1: HatSprite = TEE_SPRITE_HATS_TOP2; break;
					case 2: HatSprite = TEE_SPRITE_HATS_SIDE1; break;
					case 3: HatSprite = TEE_SPRITE_HATS_SIDE2; break;
					default: HatSprite = TEE_SPRITE_HATS_TOP1; break;
					}
					if(ResolveSprite(HatSprite, U0, V0, U1, V1))
					{
						const bool FlipX = Direction.x < 0.0f;
						SubmitQuad(Sixup.m_HatTexture, BodyPos, BodyW, BodyH, pAnim->GetBody()->m_Angle * PI * 2 + BodyAngle, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha), U0, V0, U1, V1, FlipX);
					}
				}
			}

			// ---- feet ----
			const int FootLayer = Filling ? TEE_PREVIEW_LAYER_FRONT_FEET : TEE_PREVIEW_LAYER_BACK_FEET;
			const int FootOutlineLayer = Filling ? TEE_PREVIEW_LAYER_FRONT_FEET_OUTLINE : TEE_PREVIEW_LAYER_BACK_FEET_OUTLINE;
			if((OutLine && !HasTeePreviewLayer(pInfo->m_TeeRenderFlags, FootOutlineLayer)) ||
				(!OutLine && !HasTeePreviewLayer(pInfo->m_TeeRenderFlags, FootLayer)))
				continue;

			const STextureHandle &FeetTexture = Sixup.PartTexture(SKINPART_FEET);
			if(!FeetTexture.IsValid())
				continue;

			const CAnimKeyframe *pFoot = Filling ? pAnim->GetFrontFoot() : pAnim->GetBackFoot();

			// Feet quads are 2:1 and drawn at the foot texture's natural scale
			// relative to the body: width = BaseSize * 2/3, height = BaseSize/3
			// (64×32 at BaseSize=96), matching the classic Floatee proportions.
			// (Upstream DDNet uses BaseSize/2.1 with w == h, which both stretches
			// the 64×32 texture into a square and renders the feet far smaller
			// than the body.)
			const float w = (BaseSize / 1.5f) * FeetScale.x;
			const float h = (BaseSize / 1.5f) * FeetScale.y / 2.0f;

			float U0, V0, U1, V1;
			const ETeeSprite FootSprite = OutLine ? TEE_SPRITE_FOOT_OUTLINE : TEE_SPRITE_FOOT;
			if(!ResolveSprite(FootSprite, U0, V0, U1, V1))
				continue;

			if(OutLine)
			{
				SubmitQuad(FeetTexture, Position + vec2(pFoot->m_X, pFoot->m_Y) * AnimScale, w, h, pFoot->m_Angle * PI * 2 + FeetAngle, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha), U0, V0, U1, V1);
			}
			else
			{
				const bool Indicate = !pInfo->m_GotAirJump; // airjump indicator
				float ColorScale = 1.0f;
				if(Indicate)
					ColorScale = 0.5f;
				const ColorRGBA FeetColor = Sixup.m_aColors[SKINPART_FEET];
				SubmitQuad(FeetTexture, Position + vec2(pFoot->m_X, pFoot->m_Y) * AnimScale, w, h, pFoot->m_Angle * PI * 2 + FeetAngle, ColorRGBA(FeetColor.r * ColorScale, FeetColor.g * ColorScale, FeetColor.b * ColorScale, FeetColor.a * Alpha), U0, V0, U1, V1);
			}
		}
	}

	if(m_pBackend != nullptr)
		m_pBackend->EndTee();
}

} // namespace teer

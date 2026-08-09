/*
 * tee_types.h - Core enums and POD types for the extracted Tee render pipeline.
 *
 * Self-contained replacements for:
 *   - game/client/animstate.h       (CAnimKeyframe / CAnimSequence / CAnimation)
 *   - engine/shared/protocol7.h     (protocol7::SKINPART_*)
 *   - game/client/component.h       (EMOTE_*)
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#ifndef TEE_RENDER_TEE_TYPES_H
#define TEE_RENDER_TEE_TYPES_H

#include <cstdint>

#include "tee_math.h"

namespace teer
{

// ---- emotes (from game/client/emoticon.h upstream) ------------------------
enum EEmote
{
	EMOTE_NORMAL = 0,
	EMOTE_PAIN,
	EMOTE_HAPPY,
	EMOTE_SURPRISE,
	EMOTE_ANGRY,
	EMOTE_BLINK,
	EMOTE_COUNT,
};

// ---- skin parts (protocol7) ------------------------------------------------
// Order matters: identical to protocol7::SKINPART_* in datasrc/seven/network.py
enum ESkinPart
{
	SKINPART_BODY = 0,
	SKINPART_MARKING,
	SKINPART_DECORATION,
	SKINPART_HANDS,
	SKINPART_FEET,
	SKINPART_EYES,
	NUM_SKINPARTS,
};

// Number of dummies (main + dummy), matches DDNet NUM_DUMMIES
constexpr int NUM_DUMMIES = 2;

// ---- animation data ---------------------------------------------------------
struct CAnimKeyframe
{
	float m_Time;
	float m_X;
	float m_Y;
	float m_Angle;

	CAnimKeyframe() :
		m_Time(0), m_X(0), m_Y(0), m_Angle(0) {}
	CAnimKeyframe(float Time, float X, float Y, float Angle) :
		m_Time(Time), m_X(X), m_Y(Y), m_Angle(Angle) {}
};

// Bounded sequence of keyframes (fixed capacity so it stays POD / trivially copyable).
constexpr int MAX_ANIM_KEYFRAMES = 8;

struct CAnimSequence
{
	int m_NumFrames;
	CAnimKeyframe m_aFrames[MAX_ANIM_KEYFRAMES];

	CAnimSequence() :
		m_NumFrames(0) {}
};

struct CAnimation
{
	const char *m_pName;
	CAnimSequence m_Body;
	CAnimSequence m_BackFoot;
	CAnimSequence m_FrontFoot;
	CAnimSequence m_Attach;
};

// ---- tee sprite ids (protocol7) ----------------------------------------------
// Identifiers matching the sprites defined in datasrc/seven/content.py.
enum ETeeSprite
{
	TEE_SPRITE_BODY_OUTLINE = 0,
	TEE_SPRITE_BODY,
	TEE_SPRITE_BODY_SHADOW,
	TEE_SPRITE_BODY_UPPER_OUTLINE,

	TEE_SPRITE_MARKING,

	TEE_SPRITE_DECORATION,
	TEE_SPRITE_DECORATION_OUTLINE,

	TEE_SPRITE_FOOT,
	TEE_SPRITE_FOOT_OUTLINE,

	TEE_SPRITE_EYES_NORMAL,
	TEE_SPRITE_EYES_ANGRY,
	TEE_SPRITE_EYES_PAIN,
	TEE_SPRITE_EYES_HAPPY,
	TEE_SPRITE_EYES_SURPRISE,

	TEE_SPRITE_HATS_TOP1,
	TEE_SPRITE_HATS_TOP2,
	TEE_SPRITE_HATS_SIDE1,
	TEE_SPRITE_HATS_SIDE2,

	TEE_SPRITE_BOT_BACKGROUND,
	TEE_SPRITE_BOT_FOREGROUND,
	TEE_SPRITE_BOT_GLOW,

	NUM_TEE_SPRITES,
};

} // namespace teer

#endif // TEE_RENDER_TEE_TYPES_H

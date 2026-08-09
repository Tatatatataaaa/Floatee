/*
 * tee_anim.h - Animation state for the extracted Tee render pipeline.
 *
 * Port of DDNet's game/client/animstate.h + the preset animation data that
 * upstream generates from datasrc/content.py into generated/client_data.h.
 *
 * The preset animation data (base/idle/inair/sit/walk/run) is baked in as
 * static data so no engine / generated header is required.
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#ifndef TEE_RENDER_TEE_ANIM_H
#define TEE_RENDER_TEE_ANIM_H

#include "tee_types.h"

namespace teer
{

class CAnimState
{
	CAnimKeyframe m_Body;
	CAnimKeyframe m_BackFoot;
	CAnimKeyframe m_FrontFoot;
	CAnimKeyframe m_Attach;

	static void AnimAddKeyframe(CAnimKeyframe *pSeq, const CAnimKeyframe *pAdded, float Amount);
	void AnimAdd(const CAnimState *pAdded, float Amount);

public:
	CAnimState() = default;

	const CAnimKeyframe *GetBody() const { return &m_Body; }
	const CAnimKeyframe *GetBackFoot() const { return &m_BackFoot; }
	const CAnimKeyframe *GetFrontFoot() const { return &m_FrontFoot; }
	const CAnimKeyframe *GetAttach() const { return &m_Attach; }

	// Evaluate a single animation sequence at time t.
	static void AnimSeqEval(const CAnimSequence *pSeq, float Time, CAnimKeyframe *pFrame);

	// Evaluate the full animation at time t (overwrites current state).
	void Set(const CAnimation *pAnim, float Time);

	// Add pAnim evaluated at time t, scaled by amount, onto the current state.
	void Add(const CAnimation *pAnim, float Time, float Amount);

	// Returns the classic idle pose used when a tee is not playing an action.
	static const CAnimState *GetIdle();
};

// ---- preset animations (from datasrc/content.py) -----------------------------

// "base"
extern const CAnimation s_aAnimations[]; // indexed by EAnimIndex below
enum EAnimIndex
{
	ANIM_BASE = 0,
	ANIM_IDLE,
	ANIM_INAIR,
	ANIM_SIT_LEFT,
	ANIM_SIT_RIGHT,
	ANIM_WALK,
	ANIM_RUN_LEFT,
	ANIM_RUN_RIGHT,
	ANIM_HAMMER_SWING,
	ANIM_NINJA_SWING,
	NUM_ANIMS,
};

} // namespace teer

#endif // TEE_RENDER_TEE_ANIM_H

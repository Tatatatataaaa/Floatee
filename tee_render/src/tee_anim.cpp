/*
 * tee_anim.cpp - Animation evaluation + preset data.
 *
 * Port of DDNet's game/client/animstate.cpp. The animation data below is
 * extracted verbatim from datasrc/content.py (base/idle/inair/sit/walk/run
 * plus the two weapon-swing anims which only drive the attach keyframe).
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#include "tee_anim.h"

namespace teer
{

void CAnimState::AnimSeqEval(const CAnimSequence *pSeq, float Time, CAnimKeyframe *pFrame)
{
	if(pSeq->m_NumFrames == 0)
	{
		pFrame->m_Time = 0;
		pFrame->m_X = 0;
		pFrame->m_Y = 0;
		pFrame->m_Angle = 0;
	}
	else if(pSeq->m_NumFrames == 1)
	{
		*pFrame = pSeq->m_aFrames[0];
	}
	else
	{
		const CAnimKeyframe *pFrame1 = nullptr;
		const CAnimKeyframe *pFrame2 = nullptr;
		float Blend = 0.0f;

		for(int i = 1; i < pSeq->m_NumFrames; i++)
		{
			if(pSeq->m_aFrames[i - 1].m_Time <= Time && pSeq->m_aFrames[i].m_Time >= Time)
			{
				pFrame1 = &pSeq->m_aFrames[i - 1];
				pFrame2 = &pSeq->m_aFrames[i];
				Blend = (Time - pFrame1->m_Time) / (pFrame2->m_Time - pFrame1->m_Time);
				break;
			}
		}

		if(pFrame1 != nullptr && pFrame2 != nullptr)
		{
			pFrame->m_Time = Time;
			pFrame->m_X = Mix(pFrame1->m_X, pFrame2->m_X, Blend);
			pFrame->m_Y = Mix(pFrame1->m_Y, pFrame2->m_Y, Blend);
			pFrame->m_Angle = Mix(pFrame1->m_Angle, pFrame2->m_Angle, Blend);
		}
	}
}

void CAnimState::AnimAddKeyframe(CAnimKeyframe *pSeq, const CAnimKeyframe *pAdded, float Amount)
{
	pSeq->m_X += pAdded->m_X * Amount;
	pSeq->m_Y += pAdded->m_Y * Amount;
	pSeq->m_Angle += pAdded->m_Angle * Amount;
}

void CAnimState::AnimAdd(const CAnimState *pAdded, float Amount)
{
	AnimAddKeyframe(&m_Body, pAdded->GetBody(), Amount);
	AnimAddKeyframe(&m_BackFoot, pAdded->GetBackFoot(), Amount);
	AnimAddKeyframe(&m_FrontFoot, pAdded->GetFrontFoot(), Amount);
	AnimAddKeyframe(&m_Attach, pAdded->GetAttach(), Amount);
}

void CAnimState::Set(const CAnimation *pAnim, float Time)
{
	AnimSeqEval(&pAnim->m_Body, Time, &m_Body);
	AnimSeqEval(&pAnim->m_BackFoot, Time, &m_BackFoot);
	AnimSeqEval(&pAnim->m_FrontFoot, Time, &m_FrontFoot);
	AnimSeqEval(&pAnim->m_Attach, Time, &m_Attach);
}

void CAnimState::Add(const CAnimation *pAnim, float Time, float Amount)
{
	CAnimState Add;
	Add.Set(pAnim, Time);
	AnimAdd(&Add, Amount);
}

const CAnimState *CAnimState::GetIdle()
{
	static CAnimState s_State;
	static bool s_Init = true;

	if(s_Init)
	{
		s_State.Set(&s_aAnimations[ANIM_BASE], 0.0f);
		s_State.Add(&s_aAnimations[ANIM_IDLE], 0.0f, 1.0f);
		s_Init = false;
	}

	return &s_State;
}

// ---- preset animation data (verbatim from datasrc/content.py) ----------------

static CAnimSequence MakeSeq(int Num, const CAnimKeyframe *pFrames)
{
	CAnimSequence Seq;
	Seq.m_NumFrames = Num;
	for(int i = 0; i < Num; i++)
	{
		Seq.m_aFrames[i] = pFrames[i];
	}
	return Seq;
}

#define KF(t, x, y, a) CAnimKeyframe(t, x, y, a)

// "base"
static const CAnimKeyframe aBaseBody[] = {KF(0, 0, -4, 0)};
static const CAnimKeyframe aBaseBackFoot[] = {KF(0, 0, 10, 0)};
static const CAnimKeyframe aBaseFrontFoot[] = {KF(0, 0, 10, 0)};

// "idle"
static const CAnimKeyframe aIdleBackFoot[] = {KF(0, -7, 0, 0)};
static const CAnimKeyframe aIdleFrontFoot[] = {KF(0, 7, 0, 0)};

// "inair"
static const CAnimKeyframe aInairBackFoot[] = {KF(0, -3, 0, -0.1f)};
static const CAnimKeyframe aInairFrontFoot[] = {KF(0, 3, 0, -0.1f)};

// "sit_left"
static const CAnimKeyframe aSitLeftBody[] = {KF(0, 0, 3, 0)};
static const CAnimKeyframe aSitLeftBackFoot[] = {KF(0, -12, 0, 0.1f)};
static const CAnimKeyframe aSitLeftFrontFoot[] = {KF(0, -8, 0, 0.1f)};

// "sit_right"
static const CAnimKeyframe aSitRightBody[] = {KF(0, 0, 3, 0)};
static const CAnimKeyframe aSitRightBackFoot[] = {KF(0, 12, 0, -0.1f)};
static const CAnimKeyframe aSitRightFrontFoot[] = {KF(0, 8, 0, -0.1f)};

// "walk"
static const CAnimKeyframe aWalkBody[] = {
	KF(0.0f, 0, 0, 0), KF(0.2f, 0, -1, 0), KF(0.4f, 0, 0, 0),
	KF(0.6f, 0, 0, 0), KF(0.8f, 0, -1, 0), KF(1.0f, 0, 0, 0)};
static const CAnimKeyframe aWalkBackFoot[] = {
	KF(0.0f, 8, 0, 0), KF(0.2f, -8, 0, 0), KF(0.4f, -10, -4, 0.2f),
	KF(0.6f, -8, -8, 0.3f), KF(0.8f, 4, -4, -0.2f), KF(1.0f, 8, 0, 0)};
static const CAnimKeyframe aWalkFrontFoot[] = {
	KF(0.0f, -10, -4, 0.2f), KF(0.2f, -8, -8, 0.3f), KF(0.4f, 4, -4, -0.2f),
	KF(0.6f, 8, 0, 0), KF(0.8f, 8, 0, 0), KF(1.0f, -10, -4, 0.2f)};

// "run_left"
static const CAnimKeyframe aRunLeftBody[] = {
	KF(0.0f, 0, -1, 0), KF(0.2f, 0, 0, 0), KF(0.4f, 0, -1, 0),
	KF(0.6f, 0, 0, 0), KF(0.8f, 0, 0, 0), KF(1.0f, 0, -1, 0)};
static const CAnimKeyframe aRunLeftBackFoot[] = {
	KF(0.0f, 18, -8, -0.27f), KF(0.2f, 6, 0, 0), KF(0.4f, -7, 0, 0),
	KF(0.6f, -13, -4.5f, 0.05f), KF(0.8f, 0, -8, -0.2f), KF(1.0f, 18, -8, -0.27f)};
static const CAnimKeyframe aRunLeftFrontFoot[] = {
	KF(0.0f, -11, -2.5f, 0.05f), KF(0.2f, -14, -5, 0.1f), KF(0.4f, 11, -8, -0.3f),
	KF(0.6f, 18, -8, -0.27f), KF(0.8f, 3, 0, 0), KF(1.0f, -11, -2.5f, 0.05f)};

// "run_right"
static const CAnimKeyframe aRunRightBody[] = {
	KF(0.0f, 0, -1, 0), KF(0.2f, 0, 0, 0), KF(0.4f, 0, 0, 0),
	KF(0.6f, 0, -1, 0), KF(0.8f, 0, 0, 0), KF(1.0f, 0, -1, 0)};
static const CAnimKeyframe aRunRightBackFoot[] = {
	KF(0.0f, -18, -8, 0.27f), KF(0.2f, 0, -8, 0.2f), KF(0.4f, 13, -4.5f, -0.05f),
	KF(0.6f, 7, 0, 0), KF(0.8f, -6, 0, 0), KF(1.0f, -18, -8, 0.27f)};
static const CAnimKeyframe aRunRightFrontFoot[] = {
	KF(0.0f, 11, -2.5f, -0.05f), KF(0.2f, -3, 0, 0), KF(0.4f, -18, -8, 0.27f),
	KF(0.6f, -11, -8, 0.3f), KF(0.8f, 14, -5, -0.1f), KF(1.0f, 11, -2.5f, -0.05f)};

// "hammer_swing" (attach only)
static const CAnimKeyframe aHammerSwingAttach[] = {
	KF(0.0f, 0, 0, -0.10f), KF(0.3f, 0, 0, 0.25f), KF(0.4f, 0, 0, 0.30f),
	KF(0.5f, 0, 0, 0.25f), KF(1.0f, 0, 0, -0.10f)};

// "ninja_swing" (attach only)
static const CAnimKeyframe aNinjaSwingAttach[] = {
	KF(0.00f, 0, 0, -0.25f), KF(0.10f, 0, 0, -0.05f), KF(0.15f, 0, 0, 0.35f),
	KF(0.42f, 0, 0, 0.40f), KF(0.50f, 0, 0, 0.35f), KF(1.00f, 0, 0, -0.25f)};

#undef KF

const CAnimation s_aAnimations[] = {
	{
		"base",
		MakeSeq(1, aBaseBody),
		MakeSeq(1, aBaseBackFoot),
		MakeSeq(1, aBaseFrontFoot),
		MakeSeq(0, nullptr),
	},
	{
		"idle",
		MakeSeq(0, nullptr),
		MakeSeq(1, aIdleBackFoot),
		MakeSeq(1, aIdleFrontFoot),
		MakeSeq(0, nullptr),
	},
	{
		"inair",
		MakeSeq(0, nullptr),
		MakeSeq(1, aInairBackFoot),
		MakeSeq(1, aInairFrontFoot),
		MakeSeq(0, nullptr),
	},
	{
		"sit_left",
		MakeSeq(1, aSitLeftBody),
		MakeSeq(1, aSitLeftBackFoot),
		MakeSeq(1, aSitLeftFrontFoot),
		MakeSeq(0, nullptr),
	},
	{
		"sit_right",
		MakeSeq(1, aSitRightBody),
		MakeSeq(1, aSitRightBackFoot),
		MakeSeq(1, aSitRightFrontFoot),
		MakeSeq(0, nullptr),
	},
	{
		"walk",
		MakeSeq(6, aWalkBody),
		MakeSeq(6, aWalkBackFoot),
		MakeSeq(6, aWalkFrontFoot),
		MakeSeq(0, nullptr),
	},
	{
		"run_left",
		MakeSeq(6, aRunLeftBody),
		MakeSeq(6, aRunLeftBackFoot),
		MakeSeq(6, aRunLeftFrontFoot),
		MakeSeq(0, nullptr),
	},
	{
		"run_right",
		MakeSeq(6, aRunRightBody),
		MakeSeq(6, aRunRightBackFoot),
		MakeSeq(6, aRunRightFrontFoot),
		MakeSeq(0, nullptr),
	},
	{
		"hammer_swing",
		MakeSeq(0, nullptr),
		MakeSeq(0, nullptr),
		MakeSeq(0, nullptr),
		MakeSeq(5, aHammerSwingAttach),
	},
	{
		"ninja_swing",
		MakeSeq(0, nullptr),
		MakeSeq(0, nullptr),
		MakeSeq(0, nullptr),
		MakeSeq(6, aNinjaSwingAttach),
	},
};

} // namespace teer

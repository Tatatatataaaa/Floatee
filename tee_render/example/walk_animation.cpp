/*
 * walk_animation.cpp - Export a walking tee animation using the real
 * CAnimState walk keyframes from the extracted pipeline.
 *
 * Outputs one quads.txt per frame into frames/quads_NN.txt; a companion
 * Python script (render_walk_gif.py) rasterizes them into a GIF.
 */
#include <tee_anim.h>
#include <tee_render_info.h>
#include <tee_renderer.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

using namespace teer;

class CExportBackend : public ITeeRenderBackend
{
public:
	void BeginTee() override {}
	void EndTee() override {}

	void DrawQuad(const STeeQuad &Quad) override
	{
		m_Quads.push_back(Quad);
	}

	void WriteQuads(const char *pPath) const
	{
		std::ofstream Out(pPath);
		for(const auto &Q : m_Quads)
		{
			Out << Q.m_Texture.m_Id << ' '
			    << Q.m_Position.x << ' ' << Q.m_Position.y << ' '
			    << Q.m_Width << ' ' << Q.m_Height << ' '
			    << Q.m_Rotation << ' '
			    << Q.m_Color.r << ' ' << Q.m_Color.g << ' ' << Q.m_Color.b << ' ' << Q.m_Color.a << ' '
			    << Q.m_U0 << ' ' << Q.m_V0 << ' ' << Q.m_U1 << ' ' << Q.m_V1 << ' '
			    << (Q.m_FlipX ? 1 : 0) << '\n';
		}
	}

	void Clear() { m_Quads.clear(); }

private:
	std::vector<STeeQuad> m_Quads;
};

static void BuildHollowKnightSkin(STeeRenderInfo &Info)
{
	Info.Reset();
	Info.m_Size = 64.0f;
	Info.m_GotAirJump = true;

	SSixupSkin &Sixup = Info.m_aSixup[0];
	Sixup.Reset();

	Sixup.m_aOriginalTextures[SKINPART_BODY] = STextureHandle(1);
	Sixup.m_aOriginalTextures[SKINPART_EYES] = STextureHandle(1);
	Sixup.m_aOriginalTextures[SKINPART_FEET] = STextureHandle(1);
	Info.m_Skin6EyePair = true;
	Info.m_Skin6EyeSeparationScale = 1.0f;

	Sixup.m_aColors[SKINPART_BODY] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	Sixup.m_aColors[SKINPART_EYES] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	Sixup.m_aColors[SKINPART_FEET] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
}

static void ConfigureSpriteRegions(CTeeRenderer &Renderer)
{
	const float TW = 4096.0f, TH = 2048.0f;
	Renderer.SetSpriteRegion(TEE_SPRITE_BODY, SSpriteRegion(0.0f / TW, 0.0f / TH, 1536.0f / TW, 1536.0f / TH, 1536.0f, 1536.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_BODY_OUTLINE, SSpriteRegion(1536.0f / TW, 0.0f / TH, 3072.0f / TW, 1536.0f / TH, 1536.0f, 1536.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_FOOT, SSpriteRegion(3072.0f / TW, 512.0f / TH, 4096.0f / TW, 1024.0f / TH, 1024.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_FOOT_OUTLINE, SSpriteRegion(3072.0f / TW, 1024.0f / TH, 4096.0f / TW, 1536.0f / TH, 1024.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_NORMAL, SSpriteRegion(1024.0f / TW, 1536.0f / TH, 1536.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_ANGRY, SSpriteRegion(1536.0f / TW, 1536.0f / TH, 2048.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_PAIN, SSpriteRegion(2048.0f / TW, 1536.0f / TH, 2560.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_HAPPY, SSpriteRegion(2560.0f / TW, 1536.0f / TH, 3072.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_SURPRISE, SSpriteRegion(3584.0f / TW, 1536.0f / TH, 4096.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
}

static void CreateDir(const char *pPath)
{
#ifdef _WIN32
	_mkdir(pPath);
#else
	mkdir(pPath, 0755);
#endif
}

int main()
{
	CreateDir("example/frames");

	CExportBackend Backend;
	CTeeRenderer Renderer(&Backend);
	ConfigureSpriteRegions(Renderer);

	STeeRenderInfo Info;
	BuildHollowKnightSkin(Info);
	Info.m_Size = 512.0f;

	const float CANVAS_W = 960.0f;
	const float CANVAS_H = 540.0f;
	const vec2 TeePos(CANVAS_W * 0.5f, CANVAS_H * 0.5f + 60.0f);

	constexpr int FRAME_COUNT = 12;
	for(int i = 0; i < FRAME_COUNT; i++)
	{
		Backend.Clear();

		CAnimState State;
		State.Set(&s_aAnimations[ANIM_BASE], 0.0f);
		const float WalkTime = static_cast<float>(i) / FRAME_COUNT;
		State.Add(&s_aAnimations[ANIM_WALK], WalkTime, 1.0f);

		Renderer.RenderTee(&State, &Info, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeePos);

		char aBuf[256];
		std::snprintf(aBuf, sizeof(aBuf), "example/frames/quads_%02d.txt", i);
		Backend.WriteQuads(aBuf);
		std::printf("Frame %02d: %s\n", i, aBuf);
	}

	// Write a tiny metadata file for the Python rasterizer.
	{
		std::ofstream Out("example/frames/meta.txt");
		Out << CANVAS_W << ' ' << CANVAS_H << ' ' << FRAME_COUNT << '\n';
	}

	return 0;
}

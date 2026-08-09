/*
 * main.cpp - Render a real protocol-7 4K-template skin with the extracted pipeline.
 *
 * The pipeline (CTeeRenderer) computes every quad; the export backend writes:
 *   - skin_render.svg : visually inspectable SVG (clips the UV regions of the
 *                       skin atlas into textured, rotated quads).
 *   - quads.txt       : machine-readable quad list for the companion
 *                       render_skin.py script (PIL rasterizer -> skin_render.png).
 *
 * The skin atlas is the full-resolution protocol-7 4K template (4096x2048),
 * 512px tiles:
 *   head base      A  tile(0,0) 3x3 -> px(   0,   0)-(1536,1536)
 *   head outline   B  tile(3,0) 3x3 -> px(1536,   0)-(3072,1536)
 *   hand base      C  tile(6,0) 1x1 -> px(3072,   0)-(3584, 512)
 *   hand outline   D  tile(7,0) 1x1 -> px(3584,   0)-(4096, 512)
 *   foot base      E  tile(6,1) 2x1 -> px(3072, 512)-(4096,1024)
 *   foot outline   F  tile(6,2) 2x1 -> px(3072,1024)-(4096,1536)
 *   eye normal G1  tile(2,3) 1x1 -> px(1024,1536)-(1536,2048)
 *   eye angry  G2  tile(3,3)     -> px(1536,1536)-(2048,2048)
 *   eye clumsy G3  tile(4,3)     -> px(2048,1536)-(2560,2048)
 *   eye happy  G4  tile(5,3)     -> px(2560,1536)-(3072,2048)
 *   eye dazed  H   tile(7,3)     -> px(3584,1536)-(4096,2048)
 *
 * Texture id: 1 = skin atlas (hollowknight.png). This 4K skin has feet, so the
 * feet texture is set and the renderer draws them. Eyes use the stock skin6
 * spacing formula (m_Skin6EyeSeparationScale = 1.0f), matching QmClient.
 *
 * Build & run (from tee_render/):
 *   cmake -S . -B build -G "MinGW Makefiles"
 *   cmake --build build
 *   ./build/example/tee_render_example
 */
#include <tee_anim.h>
#include <tee_emoticon.h>
#include <tee_render_info.h>
#include <tee_renderer.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace teer;

// ---------------------------------------------------------------------------
// Export backend: records quads, builds an SVG with textured clipped quads,
// and can dump the raw quad list.
// ---------------------------------------------------------------------------
class CExportBackend : public ITeeRenderBackend
{
public:
	static const char *TextureFile(uint32_t Id)
	{
		switch(Id)
		{
		case 1:
			return "hollowknight.png";
		case 2:
			return "emoticons.png";
		default:
			return "";
		}
	}

	void BeginTee() override {}
	void EndTee() override {}

	void DrawQuad(const STeeQuad &Quad) override
	{
		m_Quads.push_back(Quad);
	}

	// Build SVG. Each quad becomes a rotated group with a clipped <image> that
	// shows the texture's UV sub-region stretched over the quad's rectangle.
	std::string BuildSvg(float Width, float Height) const
	{
		std::string Svg;
		char Buf[512];
		Svg.reserve(4096);

		Svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"";
		Svg += std::to_string((int)Width);
		Svg += "\" height=\"";
		Svg += std::to_string((int)Height);
		Svg += "\" viewBox=\"0 0 ";
		Svg += std::to_string((int)Width);
		Svg += " ";
		Svg += std::to_string((int)Height);
		Svg += "\">\n<defs>\n";

		// clip paths
		int Idx = 0;
		for(const auto &Q : m_Quads)
		{
			const float ClipX = -Q.m_Width / 2.0f + Q.m_U0 * Q.m_Width;
			const float ClipY = -Q.m_Height / 2.0f + Q.m_V0 * Q.m_Height;
			const float ClipW = (Q.m_U1 - Q.m_U0) * Q.m_Width;
			const float ClipH = (Q.m_V1 - Q.m_V0) * Q.m_Height;
			std::snprintf(Buf, sizeof(Buf),
				"<clipPath id=\"cp%d\"><rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\"/></clipPath>\n",
				Idx, ClipX, ClipY, ClipW, ClipH);
			Svg += Buf;
			Idx++;
		}
		Svg += "</defs>\n";
		Svg += "<rect x=\"0\" y=\"0\" width=\"";
		Svg += std::to_string((int)Width);
		Svg += "\" height=\"";
		Svg += std::to_string((int)Height);
		Svg += "\" fill=\"#1b1e2b\"/>\n";

		// quads
		Idx = 0;
		for(const auto &Q : m_Quads)
		{
			const char *pFile = TextureFile(Q.m_Texture.m_Id);
			if(pFile[0] == '\0')
				continue;
			const float AngleDeg = Q.m_Rotation * 180.0f / PI;
			std::snprintf(Buf, sizeof(Buf),
				"<g transform=\"translate(%.2f %.2f) rotate(%.2f)\">\n",
				Q.m_Position.x, Q.m_Position.y, AngleDeg);
			Svg += Buf;
			if(Q.m_FlipX)
			{
				std::snprintf(Buf, sizeof(Buf), "  <g transform=\"scale(-1 1)\">\n");
				Svg += Buf;
			}
			std::snprintf(Buf, sizeof(Buf),
				"  <image clip-path=\"url(#cp%d)\" x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" preserveAspectRatio=\"none\" href=\"%s\"/>\n",
				Idx, -Q.m_Width / 2.0f, -Q.m_Height / 2.0f, Q.m_Width, Q.m_Height, pFile);
			Svg += Buf;
			if(Q.m_FlipX)
				Svg += "  </g>\n";
			Svg += "</g>\n";
			Idx++;
		}
		Svg += "</svg>\n";
		return Svg;
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

	int QuadCount() const { return (int)m_Quads.size(); }

private:
	std::vector<STeeQuad> m_Quads;
};

// ---------------------------------------------------------------------------
// Build the protocol-7 4K skin from the atlas texture id 1. body + eyes + feet
// all come from the atlas. Uses QmClient's stock skin6 eye-spacing formula.
// ---------------------------------------------------------------------------
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
	// Stock QmClient spacing (1.0f): this 4K skin's visible eye silhouette is
	// narrow enough that the two eyes do not overlap at the stock spacing.
	Info.m_Skin6EyeSeparationScale = 1.0f;

	// Original textures keep their baked-in colours.
	Sixup.m_aColors[SKINPART_BODY] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	Sixup.m_aColors[SKINPART_EYES] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	Sixup.m_aColors[SKINPART_FEET] = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
}

// protocol-7 4K template (4096x2048) -> UV regions.
static void ConfigureSpriteRegions(CTeeRenderer &Renderer)
{
	const float TW = 4096.0f, TH = 2048.0f;
	// head base A
	Renderer.SetSpriteRegion(TEE_SPRITE_BODY, SSpriteRegion(0.0f / TW, 0.0f / TH, 1536.0f / TW, 1536.0f / TH, 1536.0f, 1536.0f));
	// head outline B
	Renderer.SetSpriteRegion(TEE_SPRITE_BODY_OUTLINE, SSpriteRegion(1536.0f / TW, 0.0f / TH, 3072.0f / TW, 1536.0f / TH, 1536.0f, 1536.0f));
	// feet E/F
	Renderer.SetSpriteRegion(TEE_SPRITE_FOOT, SSpriteRegion(3072.0f / TW, 512.0f / TH, 4096.0f / TW, 1024.0f / TH, 1024.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_FOOT_OUTLINE, SSpriteRegion(3072.0f / TW, 1024.0f / TH, 4096.0f / TW, 1536.0f / TH, 1024.0f, 512.0f));
	// eyes G1..G4 + H
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_NORMAL, SSpriteRegion(1024.0f / TW, 1536.0f / TH, 1536.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_ANGRY, SSpriteRegion(1536.0f / TW, 1536.0f / TH, 2048.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_PAIN, SSpriteRegion(2048.0f / TW, 1536.0f / TH, 2560.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_HAPPY, SSpriteRegion(2560.0f / TW, 1536.0f / TH, 3072.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
	Renderer.SetSpriteRegion(TEE_SPRITE_EYES_SURPRISE, SSpriteRegion(3584.0f / TW, 1536.0f / TH, 4096.0f / TW, 2048.0f / TH, 512.0f, 512.0f));
	// shadow / upper-outline / marking / decoration / hat / bot stay invalid -> skipped
}

// ---------------------------------------------------------------------------
// 4K output canvas (UHD 3840x2160). Renders four tees in a row, each with an
// emoticon glued to its head. The emoticon size and head offset are scaled to
// the tee size (SetTeeSize), exactly like the in-game 64px tee relationship.
// ---------------------------------------------------------------------------
static const float CANVAS_W = 3840.0f;
static const float CANVAS_H = 2160.0f;
static const float TEE_SIZE = 256.0f;
static const float TEE_Y = CANVAS_H * 0.5f + 30.0f;

int main()
{
	CExportBackend Backend;
	CTeeRenderer Renderer(&Backend);
	ConfigureSpriteRegions(Renderer);

	STeeRenderInfo Info;
	BuildHollowKnightSkin(Info);
	Info.m_Size = TEE_SIZE;

	const CAnimState *pIdle = CAnimState::GetIdle();

	// Four tees evenly spaced across the 4K canvas.
	const float aTeeX[] = {CANVAS_W * 0.15f, CANVAS_W * 0.383f, CANVAS_W * 0.616f, CANVAS_W * 0.85f};
	for(int i = 0; i < 4; i++)
		Renderer.RenderTee(pIdle, &Info, EMOTE_HAPPY, vec2(1.0f, 0.0f), vec2(aTeeX[i], TEE_Y));

	// ---- over-head emoticons (extracted emoticon pipeline) ----
	// emoticons.png is a 4x4 atlas (512x512); texture id 2. Scale the bubble
	// with the tee so it stays glued to each head.
	CEmoticonRenderer EmoticonRenderer(&Backend, STextureHandle(2));
	EmoticonRenderer.ConfigureEmoticonGrid(4, 4);
	EmoticonRenderer.SetTeeSize(TEE_SIZE);

	// Show the full pop-in animation at four sample moments in one frame.
	// Elapsed 0.03s: just appeared, tiny and strongly wiggling.
	EmoticonRenderer.RenderEmoticon(vec2(aTeeX[0], TEE_Y), EMOTICON_HEARTS, 0.03f, 1.0f);
	// Elapsed 0.07s: mid pop-in, half size, still wiggling.
	EmoticonRenderer.RenderEmoticon(vec2(aTeeX[1], TEE_Y), EMOTICON_EXCLAMATION, 0.07f, 1.0f);
	// Elapsed 0.12s: pop-in finished, full size, wiggle fading.
	EmoticonRenderer.RenderEmoticon(vec2(aTeeX[2], TEE_Y), EMOTICON_GHOST, 0.12f, 1.0f);
	// Elapsed 1.0s: stable fully scaled emoticon.
	EmoticonRenderer.RenderEmoticon(vec2(aTeeX[3], TEE_Y), EMOTICON_QUESTION, 1.0f, 1.0f);

	// Write outputs.
	Backend.WriteQuads("quads.txt");
	{
		std::ofstream Out("skin_render.svg");
		Out << Backend.BuildSvg(CANVAS_W, CANVAS_H);
	}

	std::printf("Exported %d quads -> skin_render.svg + quads.txt\n", Backend.QuadCount());
	return 0;
}

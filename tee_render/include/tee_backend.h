/*
 * tee_backend.h - Abstract render backend for the extracted Tee render pipeline.
 *
 * Replaces DDNet's IGraphics dependency. The renderer only talks to this
 * interface, so it can be implemented on top of OpenGL, Vulkan, DirectX,
 * software rasterizers, etc.
 *
 * The backend is sprite-atlas driven: a "quad" is a rectangle on a texture
 * identified by a UV region. See README.md for the atlas layout used by the
 * provided sprite-region table.
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#ifndef TEE_RENDER_TEE_BACKEND_H
#define TEE_RENDER_TEE_BACKEND_H

#include <cstdint>

#include "tee_math.h"
#include "tee_skin.h"
#include "tee_types.h"

namespace teer
{

// A textured quad to be drawn. Coordinates are in pixels; the host backend is
// responsible for its own projection (the renderer outputs pixel-space quads).
struct STeeQuad
{
	STextureHandle m_Texture;
	vec2 m_Position; // quad center in pixels
	float m_Width;
	float m_Height;
	float m_Rotation; // radians
	ColorRGBA m_Color;

	// UV region inside the texture atlas ([0,1] range).
	float m_U0, m_V0, m_U1, m_V1;

	// Flip the quad horizontally (used for the left-facing eye / mirrored feet).
	bool m_FlipX;
};

// Abstract backend that the renderer submits quads to.
class ITeeRenderBackend
{
public:
	virtual ~ITeeRenderBackend() = default;

	// Called once per rendered tee, before any quads of that tee are submitted.
	virtual void BeginTee() = 0;

	// Called after all quads of a tee have been submitted.
	virtual void EndTee() = 0;

	// Submit a single quad. The backend should batch / flush as it sees fit.
	virtual void DrawQuad(const STeeQuad &Quad) = 0;
};

} // namespace teer

#endif // TEE_RENDER_TEE_BACKEND_H

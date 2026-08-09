/*
 * tee_math.h - Minimal math primitives for the extracted Tee render pipeline.
 *
 * This is a self-contained replacement for DDNet's base/vmath.h + base/math.h
 * so that the extracted pipeline has NO dependency on the DDNet engine.
 * Only standard C++17 is used.
 *
 * (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.
 */
#ifndef TEE_RENDER_TEE_MATH_H
#define TEE_RENDER_TEE_MATH_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace teer
{

constexpr float PI = 3.14159265358979323846f;

inline float Mix(float a, float b, float t) { return a + (b - a) * t; }
inline float Clamp(float v, float lo, float hi) { return std::clamp(v, lo, hi); }
inline float Abs(float v) { return std::fabs(v); }
inline float Min(float a, float b) { return std::min(a, b); }
inline float Max(float a, float b) { return std::max(a, b); }

// ---- vec2 ----------------------------------------------------------------
struct vec2
{
	float x;
	float y;

	vec2() :
		x(0.0f), y(0.0f) {}
	vec2(float X, float Y) :
		x(X), y(Y) {}

	vec2 operator+(const vec2 &o) const { return vec2(x + o.x, y + o.y); }
	vec2 operator-(const vec2 &o) const { return vec2(x - o.x, y - o.y); }
	vec2 operator*(float s) const { return vec2(x * s, y * s); }
	vec2 operator/(float s) const { return vec2(x / s, y / s); }
	vec2 &operator+=(const vec2 &o)
	{
		x += o.x;
		y += o.y;
		return *this;
	}
	vec2 &operator*=(float s)
	{
		x *= s;
		y *= s;
		return *this;
	}
};

inline vec2 Mix(const vec2 &a, const vec2 &b, float t) { return vec2(Mix(a.x, b.x, t), Mix(a.y, b.y, t)); }
inline vec2 operator*(float s, const vec2 &v) { return v * s; }

// ---- ColorRGBA -----------------------------------------------------------
struct ColorRGBA
{
	float r;
	float g;
	float b;
	float a;

	ColorRGBA() :
		r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
	ColorRGBA(float R, float G, float B, float A = 1.0f) :
		r(R), g(G), b(B), a(A) {}

	ColorRGBA WithAlpha(float Alpha) const { return ColorRGBA(r, g, b, a * Alpha); }
	ColorRGBA &operator=(const ColorRGBA &) = default;
};

} // namespace teer

#endif // TEE_RENDER_TEE_MATH_H

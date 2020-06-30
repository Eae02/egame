#pragma once

#include <cmath>

#include "API.hpp"

namespace eg
{
#pragma pack(push, 1)
	/**
	 * Represents a color in an undefined color space
	 */
	struct EG_API Color
	{
		float r;
		float g;
		float b;
		float a;
		
		static const Color White;
		static const Color Black;
		
		Color()
			: r(0), g(0), b(0), a(0) { }
		
		Color(float _r, float _g, float _b, float _a = 1.0f)
			: r(_r), g(_g), b(_b), a(_a) { }
		
		Color ScaleAlpha(float scale) const
		{
			return Color(r, g, b, a * scale);
		}
	};
#pragma pack(pop)
	
	/**
	 * Converts a single float value from sRGB space to linear space.
	 * \param x The value to convert.
	 * \return Returns the converted value.
	 */
	inline float SRGBToLinear(float x) {
		return x <= 0.04045f ? x * (1.0f / 12.92f) : std::pow( (x + 0.055f) * (1.0f / 1.055f), 2.4f);
	}
	
	/**
	 * Converts a single float value from linear space to sRGB space.
	 * \param x The value to convert.
	 * \return Returns the converted value.
	 */
	inline float LinearToSRGB(float x) {
		return x <= 0.0031308f ? x * 12.92f : 1.055f * ::powf( x, 1.0f / 2.4f ) - 0.055f;
	}
	
	/**
	 * Represents a color in linear color space
	 */
	struct ColorLin : Color
	{
		ColorLin() = default;
		
		explicit ColorLin(const Color& c)
			: Color(c) { }
		
		inline ColorLin(const struct ColorSRGB& c);
		
		ColorLin(float _r, float _g, float _b, float _a = 1.0f) : Color(_r, _g, _b, _a) { }
		
		ColorLin ScaleRGB(float scale) const
		{
			return ColorLin(r * scale, g * scale, b * scale, a);
		}
		
		ColorLin ScaleAlpha(float scale) const
		{
			return ColorLin(r, g, b, a * scale);
		}
		
		static ColorLin Mix(const ColorLin& c0, const ColorLin& c1, float a)
		{
			return ColorLin(glm::mix(c0.r, c1.r, a), glm::mix(c0.g, c1.g, a), glm::mix(c0.b, c1.b, a), glm::mix(c0.a, c1.a, a));
		}
	};
	
	/**
	 * Represents a color in SRGB color space
	 */
	struct ColorSRGB : Color
	{
		ColorSRGB() = default;
		
		explicit ColorSRGB(const Color& c)
			: Color(c) { }
		
		inline ColorSRGB(const struct ColorLin& c);
		
		ColorSRGB(float _r, float _g, float _b, float _a = 1.0f) : Color(_r, _g, _b, _a) { }
		
		ColorSRGB ScaleAlpha(float scale) const
		{
			return ColorLin(r, g, b, a * scale);
		}
		
		static ColorSRGB FromHex(uint32_t hex)
		{
			ColorSRGB color;
			color.r = ((hex & 0xFF0000) >> (8 * 2)) / 255.0f;
			color.g = ((hex & 0x00FF00) >> (8 * 1)) / 255.0f;
			color.b = ((hex & 0x0000FF) >> (8 * 0)) / 255.0f;
			color.a = 1;
			return color;
		}
		
		static ColorSRGB FromRGBAHex(uint32_t hex)
		{
			ColorSRGB color;
			color.r = ((hex & 0xFF000000) >> (8 * 3)) / 255.0f;
			color.g = ((hex & 0x00FF0000) >> (8 * 2)) / 255.0f;
			color.b = ((hex & 0x0000FF00) >> (8 * 1)) / 255.0f;
			color.a = ((hex & 0x000000FF) >> (8 * 0)) / 255.0f;
			return color;
		}
	};
	
	inline ColorSRGB::ColorSRGB(const ColorLin& c)
		: Color(LinearToSRGB(c.r), LinearToSRGB(c.g), LinearToSRGB(c.b), c.a) { }
	
	inline ColorLin::ColorLin(const ColorSRGB& c)
		: Color(SRGBToLinear(c.r), SRGBToLinear(c.g), SRGBToLinear(c.b), c.a) { }
}

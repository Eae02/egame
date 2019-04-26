#include "Translation.hpp"

namespace eg::graphics_api::gl
{
	GLenum Translate(BlendFunc f)
	{
		switch (f)
		{
		case BlendFunc::Add: return GL_FUNC_ADD;
		case BlendFunc::Subtract: return GL_FUNC_SUBTRACT;
		case BlendFunc::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
		case BlendFunc::Min: return GL_MIN;
		case BlendFunc::Max: return GL_MAX;
		}
		EG_UNREACHABLE
	}
	
	GLenum Translate(BlendFactor f)
	{
		switch (f)
		{
			case BlendFactor::Zero: return GL_ZERO;
			case BlendFactor::One: return GL_ONE;
			case BlendFactor::SrcColor: return GL_SRC_COLOR;
			case BlendFactor::OneMinusSrcColor: return GL_SRC_COLOR;
			case BlendFactor::DstColor: return GL_DST_COLOR;
			case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
			case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
			case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
			case BlendFactor::DstAlpha: return GL_DST_ALPHA;
			case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
			case BlendFactor::ConstantColor: return GL_CONSTANT_COLOR;
			case BlendFactor::OneMinusConstantColor: return GL_ONE_MINUS_CONSTANT_COLOR;
			case BlendFactor::ConstantAlpha: return GL_CONSTANT_COLOR;
			case BlendFactor::OneMinusConstantAlpha: return GL_ONE_MINUS_CONSTANT_ALPHA;
		}
		EG_UNREACHABLE
	}
	
	GLenum Translate(Topology t)
	{
		switch (t)
		{
		case Topology::TriangleList: return GL_TRIANGLES;
		case Topology::TriangleStrip: return GL_TRIANGLE_STRIP;
		case Topology::TriangleFan: return GL_TRIANGLE_FAN;
		case Topology::LineList: return GL_LINES;
		case Topology::LineStrip: return GL_LINE_STRIP;
		case Topology::Points: return GL_POINTS;
		case Topology::Patches: return GL_PATCHES;
		}
		EG_UNREACHABLE
	}
}

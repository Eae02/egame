#pragma once

#include "../Format.hpp"

#include <GL/gl3w.h>

namespace eg::graphics_api::gl
{
	struct Texture
	{
		GLuint texture;
		Format format;
		int dim;
		uint32_t width;
		uint32_t height;
	};
	
	inline Texture* UnwrapTexture(TextureHandle handle)
	{
		return reinterpret_cast<Texture*>(handle);
	}
}

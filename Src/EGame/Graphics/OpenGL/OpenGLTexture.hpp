#pragma once

#include "GL.hpp"
#include "../Format.hpp"

namespace eg::graphics_api::gl
{
	struct TextureView
	{
		GLuint texture;
		TextureSubresource subresource;
	};
	
	struct Texture
	{
		GLuint texture;
		std::vector<TextureView> views;
		GLenum type;
		Format format;
		int dim;
		uint32_t width;
		uint32_t height;
		uint32_t mipLevels;
		uint32_t arrayLayers;
		TextureUsage currentUsage;
		
		void BindAsStorageImage(uint32_t glBinding, const TextureSubresource& subresource);
		
		GLuint GetView(const TextureSubresource& subresource);
	};
	
	inline Texture* UnwrapTexture(TextureHandle handle)
	{
		return reinterpret_cast<Texture*>(handle);
	}
}

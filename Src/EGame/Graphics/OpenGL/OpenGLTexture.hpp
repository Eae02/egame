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
		std::optional<SamplerDescription> samplerDescription;
		GLenum type;
		Format format;
		int dim;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
		uint32_t mipLevels;
		uint32_t arrayLayers;
		uint32_t sampleCount;
		TextureUsage currentUsage;
		
		bool hasBlitFBO = false;
		GLuint blitFBO;
		
		void MaybeInitBlitFBO();
		
		void BindAsStorageImage(uint32_t glBinding, const TextureSubresource& subresource);
		
		GLuint GetView(const TextureSubresource& subresource);
		
		void ChangeUsage(TextureUsage newUsage);
	};
	
	inline Texture* UnwrapTexture(TextureHandle handle)
	{
		return reinterpret_cast<Texture*>(handle);
	}
}

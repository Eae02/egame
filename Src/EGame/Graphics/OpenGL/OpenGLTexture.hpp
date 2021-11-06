#pragma once

#include "GL.hpp"
#include "../Format.hpp"

#include <optional>

namespace eg::graphics_api::gl
{
	struct TextureView
	{
		GLuint texture;
		GLenum type;
		Format format;
		TextureSubresource subresource;
		
		//Used for fake texture views
		uint32_t generation;
		std::vector<GLuint> blitFboMipLevels; //Used to blit for fake texture views
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
		
		uint32_t generation = 0; //Incremented each time the texture is written to, used to synchronize fake texture views
		bool createFakeTextureViews = false;
		
		std::optional<GLuint> fbo;
		void LazyInitializeTextureFBO();
		
		void BindAsStorageImage(uint32_t glBinding, const TextureSubresource& subresource);
		
		GLuint GetView(const TextureSubresource& subresource, GLenum forcedViewType = 0,
		               Format differentFormat = Format::Undefined);
		
		void ChangeUsage(TextureUsage newUsage);
	};
	
	void BindTextureImpl(Texture& texture, GLuint sampler, uint32_t glBinding, const TextureSubresource& subresource,
	                     GLenum forcedViewType, Format differentFormat);
	
	inline Texture* UnwrapTexture(TextureHandle handle)
	{
		return reinterpret_cast<Texture*>(handle);
	}
}

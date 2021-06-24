#pragma once

#include "AbstractionHL.hpp"

namespace eg
{
	class ScreenRenderTexture
	{
	public:
		ScreenRenderTexture();
		
		void Invalidate()
		{
			m_texture = { };
		}
		
		TextureHandle GetTexture() const
		{
			return m_texture.handle;
		}
		
		FramebufferHandle GetFramebuffer(
			ScreenRenderTexture* depthTexture = nullptr,
			std::span<ScreenRenderTexture*> otherColorTextures = { });
		
		eg::TextureFlags textureFlags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		Format format = Format::Undefined;
		uint32_t mipLevels = 1;
		uint32_t sampleCount = 1;
		float resolutionScale = 1;
		
	private:
		void PrepareTexture();
		
		uint32_t m_generation = 0;
		Texture m_texture;
		
		uint32_t m_framebufferGenerationSum = 0;
		Framebuffer m_framebuffer;
	};
}

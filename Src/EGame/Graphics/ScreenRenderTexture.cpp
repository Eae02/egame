#include "ScreenRenderTexture.hpp"

namespace eg
{
FramebufferHandle ScreenRenderTexture::GetFramebuffer(
	ScreenRenderTexture* depthTexture, std::span<ScreenRenderTexture*> otherColorTextures)
{
	PrepareTexture();
	uint32_t generationSum = m_generation;
	if (depthTexture)
	{
		depthTexture->PrepareTexture();
		generationSum += depthTexture->m_generation;
	}
	for (ScreenRenderTexture* texture : otherColorTextures)
	{
		texture->PrepareTexture();
		generationSum += texture->m_generation;
	}

	if (generationSum != m_framebufferGenerationSum)
	{
		std::vector<FramebufferAttachment> colorAttachments(otherColorTextures.size() + 1);
		colorAttachments[0].texture = m_texture.handle;
		for (size_t i = 0; i < otherColorTextures.size(); i++)
		{
			colorAttachments[i + 1] = otherColorTextures[i]->m_texture.handle;
		}

		FramebufferCreateInfo fbCreateInfo;
		fbCreateInfo.colorAttachments = colorAttachments;

		if (depthTexture)
		{
			fbCreateInfo.depthStencilAttachment.texture = depthTexture->m_texture.handle;
		}

		m_framebuffer = eg::Framebuffer(fbCreateInfo);
	}

	return m_framebuffer.handle;
}

void ScreenRenderTexture::PrepareTexture()
{
	uint32_t wantedWidth = static_cast<uint32_t>(static_cast<float>(eg::CurrentResolutionX()) * resolutionScale);
	uint32_t wantedHeight = static_cast<uint32_t>(static_cast<float>(eg::CurrentResolutionY()) * resolutionScale);

	if (m_texture.handle == nullptr || wantedWidth != m_texture.Width() || wantedHeight != m_texture.Height())
	{
		SamplerDescription samplerDescription;
		samplerDescription.wrapU = WrapMode::ClampToEdge;
		samplerDescription.wrapV = WrapMode::ClampToEdge;
		samplerDescription.wrapW = WrapMode::ClampToEdge;

		eg::TextureCreateInfo textureCI;
		textureCI.flags = textureFlags;
		textureCI.mipLevels = mipLevels;
		textureCI.sampleCount = sampleCount;
		textureCI.format = format;
		textureCI.defaultSamplerDescription = &samplerDescription;

		m_texture = eg::Texture::Create2D(textureCI);
		m_generation++;
	}
}
} // namespace eg

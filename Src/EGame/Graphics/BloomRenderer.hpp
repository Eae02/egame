#pragma once

#include "AbstractionHL.hpp"

namespace eg
{
class EG_API BloomRenderer
{
public:
	enum class RenderTargetFlags
	{
		FullResolution = 1,
	};

	class EG_API RenderTarget
	{
	public:
		RenderTarget(
			uint32_t inputWidth, uint32_t inputHeight, uint32_t levels = 4,
			enum Format format = Format::R16G16B16A16_Float, RenderTargetFlags flags = {});

		const Texture& OutputTexture() const { return m_levels[0].m_textures[2]; }

		const Framebuffer& FirstLayerFramebuffer() const { return m_levels[0].m_framebuffers[0]; }

		uint32_t InputWidth() const { return m_inputWidth; }
		uint32_t InputHeight() const { return m_inputHeight; }
		eg::Format Format() const { return m_format; }

		bool MatchesWindowResolution() const;

		void BeginFirstLayerRenderPass(AttachmentLoadOp loadOp = AttachmentLoadOp::Clear);
		void EndFirstLayerRenderPass();

	private:
		friend class BloomRenderer;

		uint32_t m_inputWidth;
		uint32_t m_inputHeight;

		enum Format m_format;

		struct Level
		{
			Texture m_textures[3];
			Framebuffer m_framebuffers[3];

			DescriptorSet m_blurXDescriptorSet;
			DescriptorSet m_blurYDescriptorSet;
		};

		std::vector<Level> m_levels;
	};

	explicit BloomRenderer(eg::Format format);

	void Render(const glm::vec3& threshold, DescriptorSetRef inputTextureDescriptorSet, RenderTarget& renderTarget);

	void RenderNoBrightPass(RenderTarget& renderTarget);

	enum Format Format() const { return m_format; }

private:
	void SetBrightPassParameters();

	Pipeline m_brightPassPipeline;
	Pipeline m_blurPipelineX;
	Pipeline m_blurPipelineY;

	Buffer m_brightPassParametersBuffer;

	DescriptorSet m_brightPassDescriptorSet;
	DescriptorSet m_noBrightPassDescriptorSet;

	enum Format m_format;
};

EG_BIT_FIELD(BloomRenderer::RenderTargetFlags)
} // namespace eg

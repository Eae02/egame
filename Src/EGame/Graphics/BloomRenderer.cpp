#include "BloomRenderer.hpp"
#include "../../Shaders/Build/Bloom.vs.h"
#include "../../Shaders/Build/BloomBlurX.fs.h"
#include "../../Shaders/Build/BloomBlurY.fs.h"
#include "../../Shaders/Build/BloomBrightPass.fs.h"

namespace eg
{
static const DescriptorSetBinding BLUR_X_DS_BINDINGS[] = {
	DescriptorSetBinding{
		.binding = 0,
		.type = eg::BindingTypeTexture{},
		.shaderAccess = ShaderAccessFlags::Fragment,
	},
	DescriptorSetBinding{
		.binding = 1,
		.type = eg::BindingTypeTexture{},
		.shaderAccess = ShaderAccessFlags::Fragment,
	},
	DescriptorSetBinding{
		.binding = 2,
		.type = BindingTypeSampler{},
		.shaderAccess = ShaderAccessFlags::Fragment,
	},
};

static const DescriptorSetBinding BLUR_Y_DS_BINDINGS[] = {
	DescriptorSetBinding{
		.binding = 0,
		.type = eg::BindingTypeTexture{},
		.shaderAccess = ShaderAccessFlags::Fragment,
	},
	DescriptorSetBinding{
		.binding = 1,
		.type = BindingTypeSampler{},
		.shaderAccess = ShaderAccessFlags::Fragment,
	},
};

BloomRenderer::RenderTarget::RenderTarget(
	uint32_t inputWidth, uint32_t inputHeight, uint32_t levels, eg::Format format,
	BloomRenderer::RenderTargetFlags flags)
	: m_inputWidth(inputWidth), m_inputHeight(inputHeight), m_format(format), m_levels(levels)
{
	uint32_t levelWidth = inputWidth;
	uint32_t levelHeight = inputHeight;
	if (!HasFlag(flags, RenderTargetFlags::FullResolution))
	{
		levelWidth /= 2;
		levelHeight /= 2;
	}

	SamplerHandle sampler = GetSampler({
		.wrapU = WrapMode::ClampToEdge,
		.wrapV = WrapMode::ClampToEdge,
		.wrapW = WrapMode::ClampToEdge,
		.minFilter = TextureFilter::Linear,
		.magFilter = TextureFilter::Linear,
		.mipFilter = TextureFilter::Linear,
	});

	for (uint32_t l = 0; l < levels; l++)
	{
		for (uint32_t i = 0; i < 3; i++)
		{
			char labelBuffer[32];
			snprintf(labelBuffer, sizeof(labelBuffer), "Bloom:L%u:T%u", l, i);

			m_levels[l].m_textures[i] = eg::Texture::Create2D(TextureCreateInfo{
				.flags = TextureFlags::ShaderSample | TextureFlags::FramebufferAttachment,
				.mipLevels = 1,
				.width = levelWidth,
				.height = levelHeight,
				.format = format,
				.label = labelBuffer,
			});

			FramebufferAttachment colorAttachment(m_levels[l].m_textures[i].handle);
			m_levels[l].m_framebuffers[i] = eg::Framebuffer({ &colorAttachment, 1 });
		}

		m_levels[l].m_blurXDescriptorSet = DescriptorSet(BLUR_X_DS_BINDINGS);
		m_levels[l].m_blurXDescriptorSet.BindTexture(m_levels[l].m_textures[1], 0);
		m_levels[l].m_blurXDescriptorSet.BindSampler(sampler, 2);

		m_levels[l].m_blurYDescriptorSet = DescriptorSet(BLUR_Y_DS_BINDINGS);
		m_levels[l].m_blurYDescriptorSet.BindTexture(m_levels[l].m_textures[0], 0);
		m_levels[l].m_blurYDescriptorSet.BindSampler(sampler, 1);

		levelWidth /= 2;
		levelHeight /= 2;
	}

	for (uint32_t l = 0; l < levels - 1; l++)
	{
		m_levels[l].m_blurXDescriptorSet.BindTexture(m_levels[l + 1].m_textures[2], 1);
	}
	m_levels[levels - 1].m_blurXDescriptorSet.BindTexture(Texture::BlackPixel(), 1);
}

bool BloomRenderer::RenderTarget::MatchesWindowResolution() const
{
	return ToUnsigned(detail::resolutionX) == m_inputWidth && ToUnsigned(detail::resolutionY) == m_inputHeight;
}

void BloomRenderer::RenderTarget::BeginFirstLayerRenderPass(AttachmentLoadOp loadOp)
{
	RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = FirstLayerFramebuffer().handle;
	rpBeginInfo.colorAttachments[0].loadOp = loadOp;
	rpBeginInfo.colorAttachments[0].clearValue = ColorLin(Color::Black);
	DC.BeginRenderPass(rpBeginInfo);
}

void BloomRenderer::RenderTarget::EndFirstLayerRenderPass()
{
	DC.EndRenderPass();
	m_levels[0].m_textures[0].UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

BloomRenderer::BloomRenderer(eg::Format format) : m_format(format)
{
	eg::ShaderModule vertexShader(ShaderStage::Vertex, Bloom_vs_glsl);
	eg::ShaderModule brightPassSM(ShaderStage::Fragment, BloomBrightPass_fs_glsl);
	eg::ShaderModule blurXSM(ShaderStage::Fragment, BloomBlurX_fs_glsl);
	eg::ShaderModule blurYSM(ShaderStage::Fragment, BloomBlurY_fs_glsl);

	m_brightPassPipeline = eg::Pipeline::Create(GraphicsPipelineCreateInfo{
		.vertexShader = ShaderStageInfo(vertexShader.Handle()),
		.fragmentShader = ShaderStageInfo(brightPassSM.Handle()),
		.colorAttachmentFormats = { format },
		.label = "Bloom[BrightPass]",
	});

	m_blurPipelineX = eg::Pipeline::Create(GraphicsPipelineCreateInfo{
		.vertexShader = ShaderStageInfo(vertexShader.Handle()),
		.fragmentShader = ShaderStageInfo(blurXSM.Handle()),
		.colorAttachmentFormats = { format },
		.label = "Bloom[BlurX]",
	});

	m_blurPipelineY = eg::Pipeline::Create(GraphicsPipelineCreateInfo{
		.vertexShader = ShaderStageInfo(vertexShader.Handle()),
		.fragmentShader = ShaderStageInfo(blurYSM.Handle()),
		.colorAttachmentFormats = { format },
		.label = "Bloom[BlurY]",
	});

	constexpr uint32_t PARAMETERS_BUFFER_SIZE = sizeof(float) * 6;

	uint32_t brightPassParametersBufferZeroedOffset =
		RoundToNextMultiple(PARAMETERS_BUFFER_SIZE, GetGraphicsDeviceInfo().uniformBufferOffsetAlignment);

	m_brightPassParametersBuffer = Buffer(
		eg::BufferFlags::CopyDst | eg::BufferFlags::UniformBuffer, brightPassParametersBufferZeroedOffset * 2, nullptr);
	eg::DC.FillBuffer(m_brightPassParametersBuffer, brightPassParametersBufferZeroedOffset, PARAMETERS_BUFFER_SIZE, 0);

	SamplerHandle inputSampler = GetSampler({
		.wrapU = WrapMode::ClampToEdge,
		.wrapV = WrapMode::ClampToEdge,
		.wrapW = WrapMode::ClampToEdge,
		.minFilter = TextureFilter::Linear,
		.magFilter = TextureFilter::Linear,
		.mipFilter = TextureFilter::Linear,
	});

	m_brightPassDescriptorSet = DescriptorSet(m_brightPassPipeline, 0);
	m_brightPassDescriptorSet.BindSampler(inputSampler, 0);
	m_brightPassDescriptorSet.BindUniformBuffer(m_brightPassParametersBuffer, 1, 0, PARAMETERS_BUFFER_SIZE);

	m_noBrightPassDescriptorSet = DescriptorSet(m_brightPassPipeline, 0);
	m_noBrightPassDescriptorSet.BindSampler(inputSampler, 0);
	m_noBrightPassDescriptorSet.BindUniformBuffer(
		m_brightPassParametersBuffer, 1, brightPassParametersBufferZeroedOffset, PARAMETERS_BUFFER_SIZE);
}

void BloomRenderer::RenderNoBrightPass(RenderTarget& renderTarget)
{
	EG_ASSERT(renderTarget.Format() == m_format);

	// Downscales texture 0
	for (uint32_t l = 1; l < renderTarget.m_levels.size(); l++)
	{
		RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.framebuffer = renderTarget.m_levels[l].m_framebuffers[0].handle;
		rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Discard;
		DC.BeginRenderPass(rpBeginInfo);

		DC.BindPipeline(m_brightPassPipeline);
		DC.BindDescriptorSet(m_noBrightPassDescriptorSet, 0);
		DC.BindDescriptorSet(renderTarget.m_levels[l - 1].m_textures[0].GetFragmentShaderSampleDescriptorSet(), 1);

		DC.Draw(0, 3, 0, 1);

		DC.EndRenderPass();

		renderTarget.m_levels[l].m_textures[0].UsageHint(
			eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	}

	// Y-blurring from texture 0 to texture 1
	for (uint32_t l = 0; l < renderTarget.m_levels.size(); l++)
	{
		RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.framebuffer = renderTarget.m_levels[l].m_framebuffers[1].handle;
		rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Discard;
		DC.BeginRenderPass(rpBeginInfo);

		DC.BindPipeline(m_blurPipelineY);
		DC.BindDescriptorSet(renderTarget.m_levels[l].m_blurYDescriptorSet, 0);

		DC.Draw(0, 3, 0, 1);

		DC.EndRenderPass();

		renderTarget.m_levels[l].m_textures[1].UsageHint(
			eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	}

	// Vertical blurring from texture 1 to texture 2, followed by upscaling of texture 2
	for (int l = ToInt(renderTarget.m_levels.size()) - 1; l >= 0; l--)
	{
		RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.framebuffer = renderTarget.m_levels[l].m_framebuffers[2].handle;
		rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Discard;
		DC.BeginRenderPass(rpBeginInfo);

		DC.BindPipeline(m_blurPipelineX);
		DC.BindDescriptorSet(renderTarget.m_levels[l].m_blurXDescriptorSet, 0);

		DC.Draw(0, 3, 0, 1);

		DC.EndRenderPass();

		renderTarget.m_levels[l].m_textures[2].UsageHint(
			eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	}
}

void BloomRenderer::Render(
	const glm::vec3& threshold, DescriptorSetRef inputTextureDescriptorSet, RenderTarget& renderTarget)
{
	EG_ASSERT(renderTarget.Format() == m_format);

	const float parameters[4] = { threshold.x, threshold.y, threshold.z, 0.0f };
	m_brightPassParametersBuffer.DCUpdateData<float>(0, parameters);
	m_brightPassParametersBuffer.UsageHint(BufferUsage::UniformBuffer, ShaderAccessFlags::Fragment);

	renderTarget.BeginFirstLayerRenderPass(AttachmentLoadOp::Discard);

	DC.BindPipeline(m_brightPassPipeline);
	DC.BindDescriptorSet(m_brightPassDescriptorSet, 0);
	DC.BindDescriptorSet(inputTextureDescriptorSet, 1);

	DC.Draw(0, 3, 0, 1);

	renderTarget.EndFirstLayerRenderPass();

	RenderNoBrightPass(renderTarget);
}
} // namespace eg

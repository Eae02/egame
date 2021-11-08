#include "BloomRenderer.hpp"
#include "../../Shaders/Build/Bloom.vs.h"
#include "../../Shaders/Build/BloomBrightPass.fs.h"
#include "../../Shaders/Build/BloomBlurX.fs.h"
#include "../../Shaders/Build/BloomBlurY.fs.h"

namespace eg
{
	BloomRenderer::RenderTarget::RenderTarget(uint32_t inputWidth, uint32_t inputHeight, uint32_t levels, Format format)
		: m_inputWidth(inputWidth), m_inputHeight(inputHeight), m_levels(levels)
	{
		uint32_t levelWidth = inputWidth;
		uint32_t levelHeight = inputHeight;
		for (uint32_t l = 0; l < levels; l++)
		{
			levelWidth /= 2;
			levelHeight /= 2;
			
			TextureCreateInfo textureCI;
			textureCI.width = levelWidth;
			textureCI.height = levelHeight;
			textureCI.mipLevels = 1;
			textureCI.format = format;
			textureCI.flags = TextureFlags::ShaderSample | TextureFlags::FramebufferAttachment;
			
			for (uint32_t i = 0; i < 3; i++)
			{
				char labelBuffer[32];
				snprintf(labelBuffer, sizeof(labelBuffer), "Bloom:L%u:T%u", l, i);
				textureCI.label = labelBuffer;
				m_levels[l].m_textures[i] = eg::Texture::Create2D(textureCI);
				FramebufferAttachment colorAttachment(m_levels[l].m_textures[i].handle);
				m_levels[l].m_framebuffers[i] = eg::Framebuffer({ &colorAttachment, 1 });
			}
		}
	}
	
	BloomRenderer::BloomRenderer()
	{
		eg::ShaderModule vertexShader(ShaderStage::Vertex, Bloom_vs_glsl);
		eg::ShaderModule brightPassSM(ShaderStage::Fragment, BloomBrightPass_fs_glsl);
		eg::ShaderModule blurXSM(ShaderStage::Fragment, BloomBlurX_fs_glsl);
		eg::ShaderModule blurYSM(ShaderStage::Fragment, BloomBlurY_fs_glsl);
		
		GraphicsPipelineCreateInfo brightPassPipelineCI;
		brightPassPipelineCI.vertexShader.shaderModule = vertexShader.Handle();
		brightPassPipelineCI.fragmentShader.shaderModule = brightPassSM.Handle();
		m_brightPassPipeline = eg::Pipeline::Create(brightPassPipelineCI);
		
		GraphicsPipelineCreateInfo blurPipelineXCI;
		blurPipelineXCI.vertexShader.shaderModule = vertexShader.Handle();
		blurPipelineXCI.fragmentShader.shaderModule = blurXSM.Handle();
		m_blurPipelineX = eg::Pipeline::Create(blurPipelineXCI);
		
		GraphicsPipelineCreateInfo blurPipelineYCI;
		blurPipelineYCI.vertexShader.shaderModule = vertexShader.Handle();
		blurPipelineYCI.fragmentShader.shaderModule = blurYSM.Handle();
		m_blurPipelineY = eg::Pipeline::Create(blurPipelineYCI);
		
		SamplerDescription inputSamplerDesc;
		inputSamplerDesc.wrapU = WrapMode::ClampToEdge;
		inputSamplerDesc.wrapV = WrapMode::ClampToEdge;
		inputSamplerDesc.wrapW = WrapMode::ClampToEdge;
		inputSamplerDesc.minFilter = TextureFilter::Linear;
		inputSamplerDesc.magFilter = TextureFilter::Linear;
		inputSamplerDesc.mipFilter = TextureFilter::Linear;
		m_inputSampler = Sampler(inputSamplerDesc);
		
		TextureCreateInfo blackPixelTexCI;
		blackPixelTexCI.width = 1;
		blackPixelTexCI.height = 1;
		blackPixelTexCI.mipLevels = 1;
		blackPixelTexCI.format = Format::R8G8B8A8_UNorm;
		blackPixelTexCI.flags = TextureFlags::ShaderSample | TextureFlags::CopyDst;
		m_blackPixelTexture = Texture::Create2D(blackPixelTexCI);
		
		UploadBuffer uploadBuffer = GetTemporaryUploadBuffer(4);
		uint8_t* uploadBufferMem = static_cast<uint8_t*>(uploadBuffer.Map());
		std::fill_n(uploadBufferMem, 4, 0);
		uploadBuffer.Flush();
		
		TextureRange textureRange = { };
		textureRange.sizeX = 1;
		textureRange.sizeY = 1;
		textureRange.sizeZ = 1;
		DC.SetTextureData(m_blackPixelTexture, textureRange, uploadBuffer.buffer, uploadBuffer.offset);
		
		m_blackPixelTexture.UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Fragment);
	}
	
	void BloomRenderer::Render(const glm::vec3& threshold, TextureRef inputTexture, RenderTarget& renderTarget) const
	{
		//Bright pass from input to texture 0 of level 0
		{
			RenderPassBeginInfo rpBeginInfo;
			rpBeginInfo.framebuffer = renderTarget.m_levels[0].m_framebuffers[0].handle;
			rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Discard;
			//rpBeginInfo.colorAttachments[0].finalUsage = TextureUsage::ShaderSample;
			DC.BeginRenderPass(rpBeginInfo);
			
			DC.BindPipeline(m_brightPassPipeline);
			DC.BindTexture(inputTexture, 0, 0, &m_inputSampler);
			
			const float pc[] = { threshold.r, threshold.g, threshold.b, 0.0f };
			DC.PushConstants(0, sizeof(pc), pc);
			
			DC.Draw(0, 3, 0, 1);
			
			DC.EndRenderPass();
			
			renderTarget.m_levels[0].m_textures[0].UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
		}
		
		//Downscales texture 0
		for (uint32_t l = 1; l < renderTarget.m_levels.size(); l++)
		{
			RenderPassBeginInfo rpBeginInfo;
			rpBeginInfo.framebuffer = renderTarget.m_levels[l].m_framebuffers[0].handle;
			rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Discard;
			//rpBeginInfo.colorAttachments[0].finalUsage = TextureUsage::ShaderSample;
			DC.BeginRenderPass(rpBeginInfo);
			
			DC.BindPipeline(m_brightPassPipeline);
			DC.BindTexture(renderTarget.m_levels[l - 1].m_textures[0], 0, 0, &m_inputSampler);
			
			const float pc[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			DC.PushConstants(0, sizeof(pc), pc);
			
			DC.Draw(0, 3, 0, 1);
			
			DC.EndRenderPass();
			
			renderTarget.m_levels[l].m_textures[0].UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
		}
		
		//Y-blurring from texture 0 to texture 1
		for (uint32_t l = 0; l < renderTarget.m_levels.size(); l++)
		{
			RenderPassBeginInfo rpBeginInfo;
			rpBeginInfo.framebuffer = renderTarget.m_levels[l].m_framebuffers[1].handle;
			rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Discard;
			//rpBeginInfo.colorAttachments[0].finalUsage = TextureUsage::ShaderSample;
			DC.BeginRenderPass(rpBeginInfo);
			
			DC.BindPipeline(m_blurPipelineY);
			
			DC.BindTexture(renderTarget.m_levels[l].m_textures[0], 0, 0, &m_inputSampler);
			
			const float pixelWidth = 1.0f / (float)renderTarget.m_levels[l].m_textures[0].Width();
			const float pixelHeight = 1.0f / (float)renderTarget.m_levels[l].m_textures[0].Height();
			const float pc[] = { pixelWidth, pixelHeight };
			DC.PushConstants(0, sizeof(pc), pc);
			
			DC.Draw(0, 3, 0, 1);
			
			DC.EndRenderPass();
			
			renderTarget.m_levels[l].m_textures[1].UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
		}
		
		//Vertical blurring from texture 1 to texture 2, followed by upscaling of texture 2
		for (int l = (int)renderTarget.m_levels.size() - 1; l >= 0; l--)
		{
			RenderPassBeginInfo rpBeginInfo;
			rpBeginInfo.framebuffer = renderTarget.m_levels[l].m_framebuffers[2].handle;
			rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Discard;
			//rpBeginInfo.colorAttachments[0].finalUsage = TextureUsage::ShaderSample;
			DC.BeginRenderPass(rpBeginInfo);
			
			DC.BindPipeline(m_blurPipelineX);
			
			DC.BindTexture(renderTarget.m_levels[l].m_textures[1], 0, 0, &m_inputSampler);
			
			if (l + 1 == (int)renderTarget.m_levels.size())
				DC.BindTexture(m_blackPixelTexture, 0, 1, &m_inputSampler);
			else
				DC.BindTexture(renderTarget.m_levels[l + 1].m_textures[2], 0, 1, &m_inputSampler);
			
			const float pixelWidth = 1.0f / (float)renderTarget.m_levels[l].m_textures[0].Width();
			const float pixelHeight = 1.0f / (float)renderTarget.m_levels[l].m_textures[0].Height();
			const float pc[] = { pixelWidth, pixelHeight };
			DC.PushConstants(0, sizeof(pc), pc);
			
			DC.Draw(0, 3, 0, 1);
			
			DC.EndRenderPass();
			
			renderTarget.m_levels[l].m_textures[2].UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
		}
	}
}

#include "BloomRenderer.hpp"
#include "../../Shaders/Build/BloomBrightPass.cs.h"
#include "../../Shaders/Build/BloomBlur.cs.h"
#include "../../Shaders/Build/BloomUpscale.cs.h"

namespace eg
{
	BloomRenderer::RenderTarget::RenderTarget(uint32_t inputWidth, uint32_t inputHeight, uint32_t levels)
		: m_inputWidth(inputWidth), m_inputHeight(inputHeight)
	{
		TextureCreateInfo textureCI;
		textureCI.width = inputWidth / 2;
		textureCI.height = inputHeight / 2;
		textureCI.mipLevels = levels;
		textureCI.format = FORMAT;
		textureCI.flags = TextureFlags::ShaderSample | TextureFlags::StorageImage | TextureFlags::ManualBarrier;
		
		m_mainTexture = eg::Texture::Create2D(textureCI);
		m_auxTexture = eg::Texture::Create2D(textureCI);
	}
	
	static constexpr uint32_t WORK_GROUP_SIZE_X = 32;
	
	BloomRenderer::BloomRenderer()
	{
		m_workGroupSizeY = 4;
		while (m_workGroupSizeY <= GetGraphicsDeviceInfo().maxComputeWorkGroupSize[1] &&
		       WORK_GROUP_SIZE_X * m_workGroupSizeY <= GetGraphicsDeviceInfo().maxComputeWorkGroupInvocations)
		{
			m_workGroupSizeY += 4;
		}
		m_workGroupSizeY -= 4;
		
		eg::SpecializationConstantEntry specConstants[1];
		specConstants[0].constantID = 0;
		specConstants[0].offset = 0;
		specConstants[0].size = sizeof(uint32_t);
		
		eg::ShaderModule brightPassSM(ShaderStage::Compute, BloomBrightPass_cs_glsl);
		eg::ShaderModule blurSM(ShaderStage::Compute, BloomBlur_cs_glsl);
		eg::ShaderModule upscaleSM(ShaderStage::Compute, BloomUpscale_cs_glsl);
		
		ComputePipelineCreateInfo brightPassPipelineCI;
		brightPassPipelineCI.computeShader.shaderModule = brightPassSM.Handle();
		brightPassPipelineCI.computeShader.specConstants = specConstants;
		brightPassPipelineCI.computeShader.specConstantsData = &m_workGroupSizeY;
		brightPassPipelineCI.computeShader.specConstantsDataSize = sizeof(uint32_t);
		m_brightPassPipeline = eg::Pipeline::Create(brightPassPipelineCI);
		
		ComputePipelineCreateInfo blurPipelineCI;
		blurPipelineCI.computeShader.shaderModule = blurSM.Handle();
		blurPipelineCI.computeShader.specConstants = specConstants;
		blurPipelineCI.computeShader.specConstantsData = &m_workGroupSizeY;
		blurPipelineCI.computeShader.specConstantsDataSize = sizeof(uint32_t);
		m_blurPipeline = eg::Pipeline::Create(blurPipelineCI);
		
		ComputePipelineCreateInfo upscalePipelineCI;
		upscalePipelineCI.computeShader.shaderModule = upscaleSM.Handle();
		upscalePipelineCI.computeShader.specConstants = specConstants;
		upscalePipelineCI.computeShader.specConstantsData = &m_workGroupSizeY;
		upscalePipelineCI.computeShader.specConstantsDataSize = sizeof(uint32_t);
		m_upscalePipeline = eg::Pipeline::Create(upscalePipelineCI);
		
		SamplerDescription inputSamplerDesc;
		inputSamplerDesc.wrapU = WrapMode::ClampToEdge;
		inputSamplerDesc.wrapV = WrapMode::ClampToEdge;
		inputSamplerDesc.wrapW = WrapMode::ClampToEdge;
		inputSamplerDesc.minFilter = TextureFilter::Linear;
		inputSamplerDesc.magFilter = TextureFilter::Linear;
		inputSamplerDesc.mipFilter = TextureFilter::Linear;
		m_inputSampler = Sampler(inputSamplerDesc);
	}
	
	void BloomRenderer::Render(const glm::vec3& threshold, TextureRef inputTexture, RenderTarget& renderTarget) const
	{
		auto Dispatch = [&] (uint32_t width, uint32_t height)
		{
			DC.DispatchCompute((width + WORK_GROUP_SIZE_X - 1) / WORK_GROUP_SIZE_X,
			                   (height + m_workGroupSizeY - 1) / m_workGroupSizeY, 1);
		};
		
		TextureBarrier barrier;
		
		//Prepares the entire main texture for ILS Write
		barrier.newAccess = ShaderAccessFlags::Compute;
		barrier.oldAccess = ShaderAccessFlags::Compute;
		barrier.oldUsage = TextureUsage::Undefined;
		barrier.newUsage = TextureUsage::ILSWrite;
		barrier.subresource.firstMipLevel = 0;
		barrier.subresource.numMipLevels = REMAINING_SUBRESOURCE;
		DC.Barrier(renderTarget.m_mainTexture, barrier);
		
		//Bright pass from input to first mip of the main texture
		{
			DC.BindPipeline(m_brightPassPipeline);
			DC.BindTexture(inputTexture, 0, 0, &m_inputSampler);
			DC.BindStorageImage(renderTarget.m_mainTexture, 0, 1, { 0, 0, 1 });
			
			const float pc[] = { threshold.r, threshold.g, threshold.b, 0.0f };
			DC.PushConstants(0, sizeof(pc), pc);
			
			Dispatch(renderTarget.m_mainTexture.Width(), renderTarget.m_mainTexture.Height());
			
			//Prepares the first layer of the main texture for shader sample
			barrier.oldUsage = TextureUsage::ILSWrite;
			barrier.newUsage = TextureUsage::ShaderSample;
			barrier.subresource.firstMipLevel = 0;
			barrier.subresource.numMipLevels = 1;
			DC.Barrier(renderTarget.m_mainTexture, barrier);
		}
		
		//Downscales the main texture
		const float pc0[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		DC.PushConstants(0, sizeof(pc0), pc0);
		for (uint32_t l = 1; l < renderTarget.m_mainTexture.MipLevels(); l++)
		{
			DC.BindTexture(renderTarget.m_mainTexture, 0, 0, &m_inputSampler, { l - 1, 1, 0, 1 });
			DC.BindStorageImage(renderTarget.m_mainTexture, 0, 1, { l, 0, 1 });
			
			uint32_t outWidth = renderTarget.m_mainTexture.Width() >> l;
			uint32_t outHeight = renderTarget.m_mainTexture.Height() >> l;
			
			Dispatch(outWidth, outHeight);
			
			//Prepares the l:th layer of the main texture for shader sample
			barrier.oldUsage = TextureUsage::ILSWrite;
			barrier.newUsage = TextureUsage::ShaderSample;
			barrier.subresource.firstMipLevel = l;
			barrier.subresource.numMipLevels = 1;
			DC.Barrier(renderTarget.m_mainTexture, barrier);
		}
		
		//Prepares the entire aux texture for ILS Write
		barrier.oldUsage = TextureUsage::Undefined;
		barrier.newUsage = TextureUsage::ILSWrite;
		barrier.subresource.firstMipLevel = 0;
		barrier.subresource.numMipLevels = REMAINING_SUBRESOURCE;
		DC.Barrier(renderTarget.m_auxTexture, barrier);
		
		DC.BindPipeline(m_blurPipeline);
		
		//Horizontal blurring from the main texture to the aux texture
		for (uint32_t l = 0; l < renderTarget.m_mainTexture.MipLevels(); l++)
		{
			DC.BindTexture(renderTarget.m_mainTexture, 0, 0, &m_inputSampler, { l, 1, 0, 1 });
			DC.BindStorageImage(renderTarget.m_auxTexture, 0, 1, { l, 0, 1 });
			
			const uint32_t outWidth = renderTarget.m_mainTexture.Width() >> l;
			const uint32_t outHeight = renderTarget.m_mainTexture.Height() >> l;
			const float pixelWidth = 1.0f / (float)outWidth;
			const float pixelHeight = 1.0f / (float)outHeight;
			
			const float pc[] = { pixelWidth, 0.0f, pixelWidth, pixelHeight };
			DC.PushConstants(0, sizeof(pc), pc);
			
			Dispatch(outWidth, outHeight);
		}
		
		//Prepares the entire aux texture for shader sample
		barrier.oldUsage = TextureUsage::ILSWrite;
		barrier.newUsage = TextureUsage::ShaderSample;
		DC.Barrier(renderTarget.m_auxTexture, barrier);
		
		//Prepares the entire main texture for ILS Read Write
		barrier.oldUsage = TextureUsage::ShaderSample;
		barrier.newUsage = TextureUsage::ILSReadWrite;
		DC.Barrier(renderTarget.m_mainTexture, barrier);
		
		//Vertical blurring from the aux texture to the main texture
		for (uint32_t l = 0; l < renderTarget.m_mainTexture.MipLevels(); l++)
		{
			DC.BindTexture(renderTarget.m_auxTexture, 0, 0, &m_inputSampler, { l, 1, 0, 1 });
			DC.BindStorageImage(renderTarget.m_mainTexture, 0, 1, { l, 0, 1 });
			
			const uint32_t outWidth = renderTarget.m_mainTexture.Width() >> l;
			const uint32_t outHeight = renderTarget.m_mainTexture.Height() >> l;
			const float pixelWidth = 1.0f / (float)outWidth;
			const float pixelHeight = 1.0f / (float)outHeight;
			
			const float pc[] = { 0.0f, pixelHeight, pixelWidth, pixelHeight };
			DC.PushConstants(0, sizeof(pc), pc);
			
			Dispatch(outWidth, outHeight);
		}
		
		//Upscales the main texture
		DC.BindPipeline(m_upscalePipeline);
		for (uint32_t l = renderTarget.m_mainTexture.MipLevels() - 1; l > 0; l--)
		{
			//Prepares the l:th layer of the main texture for shader sample
			barrier.oldUsage = TextureUsage::ILSReadWrite;
			barrier.newUsage = TextureUsage::ShaderSample;
			barrier.subresource.firstMipLevel = l;
			barrier.subresource.numMipLevels = 1;
			DC.Barrier(renderTarget.m_mainTexture, barrier);
			
			DC.BindTexture(renderTarget.m_mainTexture, 0, 0, &m_inputSampler, { l, 1, 0, 1 });
			DC.BindStorageImage(renderTarget.m_mainTexture, 0, 1, { l - 1, 0, 1 });
			
			const uint32_t outWidth = renderTarget.m_mainTexture.Width() >> (l - 1);
			const uint32_t outHeight = renderTarget.m_mainTexture.Height() >> (l - 1);
			
			const float pc[] = { 1.0f / (float)outWidth, 1.0f / (float)outHeight };
			DC.PushConstants(0, sizeof(pc), pc);
			
			Dispatch(outWidth, outHeight);
		}
		
		//Prepares the first layer of the main texture for shader sample
		barrier.newAccess = ShaderAccessFlags::Fragment;
		barrier.oldUsage = TextureUsage::ILSReadWrite;
		barrier.newUsage = TextureUsage::ShaderSample;
		barrier.subresource.firstMipLevel = 0;
		barrier.subresource.numMipLevels = 1;
		DC.Barrier(renderTarget.m_mainTexture, barrier);
	}
}

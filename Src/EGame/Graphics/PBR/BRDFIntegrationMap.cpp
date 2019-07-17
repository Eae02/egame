#include "BRDFIntegrationMap.hpp"
#include "../../../Shaders/Build/BRDFIntegration.cs.h"

namespace eg
{
	BRDFIntegrationMap::BRDFIntegrationMap(uint32_t resolution)
	{
		SamplerDescription samplerDescription;
		samplerDescription.wrapU = WrapMode::ClampToEdge;
		samplerDescription.wrapV = WrapMode::ClampToEdge;
		samplerDescription.wrapW = WrapMode::ClampToEdge;
		
		TextureCreateInfo textureCI;
		textureCI.width = resolution;
		textureCI.height = resolution;
		textureCI.flags = TextureFlags::StorageImage | TextureFlags::ShaderSample;
		textureCI.mipLevels = 1;
		textureCI.format = FORMAT;
		textureCI.defaultSamplerDescription = &samplerDescription;
		m_texture = Texture::Create2D(textureCI);
		
		ShaderModule shader(ShaderStage::Compute, BRDFIntegration_cs_glsl);
		ComputePipelineCreateInfo pipelineCI;
		pipelineCI.computeShader = shader.Handle();
		Pipeline pipeline = Pipeline::Create(pipelineCI);
		
		m_texture.UsageHint(TextureUsage::ILSWrite, ShaderAccessFlags::Compute);
		
		DC.BindPipeline(pipeline);
		DC.BindStorageImage(m_texture, 0, 0);
		
		constexpr uint32_t LOCAL_SIZE = 32;
		const uint32_t dispatchSize = (resolution + LOCAL_SIZE - 1) / LOCAL_SIZE;
		DC.DispatchCompute(dispatchSize, dispatchSize, 1);
		
		m_texture.UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Fragment);
	}
}

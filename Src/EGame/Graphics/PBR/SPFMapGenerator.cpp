#include "SPFMapGenerator.hpp"
#include "../../../Shaders/Build/SPFMapGenerator.cs.h"

namespace eg
{
	SPFMapGenerator::SPFMapGenerator()
	{
		eg::ShaderModule shaderModule(ShaderStage::Compute, SPFMapGenerator_cs_glsl);
		
		eg::ComputePipelineCreateInfo pipelineCI;
		pipelineCI.computeShader = shaderModule.Handle();
		m_pipeline = Pipeline::Create(pipelineCI);
	}
	
	void SPFMapGenerator::Generate(CommandContext& cc, const Texture& inputEnvMap, Texture& output,
		uint32_t arrayLayer, float irradianceScale) const
	{
		cc.BindPipeline(m_pipeline);
		
		cc.BindTexture(inputEnvMap, 0, 0);
		uint32_t outputRes = output.Width();
		for (uint32_t i = 0; i < output.MipLevels(); i++)
		{
			eg::TextureSubresourceLayers subresource;
			subresource.mipLevel = i;
			subresource.firstArrayLayer = arrayLayer * 6;
			subresource.numArrayLayers = 6;
			cc.BindStorageImage(output, 0, 1, subresource);
			
			const float roughness = i / (float)(output.MipLevels() - 1);
			const float roughnessSq = roughness * roughness;
			float pc[3] = { roughnessSq * roughnessSq, irradianceScale, 1.0f / outputRes };
			cc.PushConstants(0, sizeof(pc), pc);
			
			constexpr uint32_t LOCAL_SIZE = 12;
			const uint32_t dispatchSize = (outputRes + LOCAL_SIZE - 1) / LOCAL_SIZE;
			cc.DispatchCompute(dispatchSize, dispatchSize, 1);
			
			outputRes /= 2;
		}
	}
}

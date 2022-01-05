#include "IrradianceMapGenerator.hpp"
#include "../../../Shaders/Build/IrradianceMapGenerator.cs.h"

namespace eg
{
	IrradianceMapGenerator::IrradianceMapGenerator()
	{
		eg::ShaderModule shaderModule(ShaderStage::Compute, IrradianceMapGenerator_cs_glsl);
		
		eg::ComputePipelineCreateInfo pipelineCI;
		pipelineCI.computeShader = shaderModule.Handle();
		m_pipeline = Pipeline::Create(pipelineCI);
	}
	
	void IrradianceMapGenerator::Generate(CommandContext& cc, const Texture& inputEnvMap, Texture& output,
		uint32_t arrayLayer, float irradianceScale) const
	{
		cc.BindPipeline(m_pipeline);
		
		cc.BindTexture(inputEnvMap, 0, 0);
		
		eg::TextureSubresource subresource;
		subresource.firstMipLevel = 0;
		subresource.numMipLevels = 1;
		subresource.firstArrayLayer = arrayLayer * 6;
		subresource.numArrayLayers = 6;
		cc.BindStorageImage(output, 0, 1, subresource);
		
		float pc[2] = { irradianceScale, 1.0f / (float)output.Width() };
		cc.PushConstants(0, sizeof(pc), pc);
		
		constexpr uint32_t LOCAL_SIZE = 12;
		const uint32_t dispatchX = (output.Width() + LOCAL_SIZE - 1) / LOCAL_SIZE;
		const uint32_t dispatchY = (output.Height() + LOCAL_SIZE - 1) / LOCAL_SIZE;
		cc.DispatchCompute(dispatchX, dispatchY, 1);
	}
}

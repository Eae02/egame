#include "Pipeline.hpp"
#include "ShaderModule.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::graphics_api::vk
{
	struct ComputePipeline : AbstractPipeline
	{
		ShaderModule* shaderModule;
		VkPipeline pipeline;
		
		void Free() override;
		
		void Bind(CommandContextHandle cc) override;
	};
	
	static ConcurrentObjectPool<ComputePipeline> computePipelinesPool;
	
	void ComputePipeline::Free()
	{
		shaderModule->UnRef();
		
		vkDestroyPipeline(ctx.device, pipeline, nullptr);
		
		computePipelinesPool.Delete(this);
	}
	
	PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo)
	{
		ComputePipeline* pipeline = computePipelinesPool.New();
		pipeline->refCount = 1;
		pipeline->bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
		
		VkComputePipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pipelineCreateInfo.basePipelineIndex = -1;
		
		ShaderModule* shaderModule = UnwrapShaderModule(createInfo.computeShader);
		shaderModule->ref++;
		InitShaderStageCreateInfo(pipelineCreateInfo.stage, shaderModule->module, VK_SHADER_STAGE_COMPUTE_BIT);
		
		pipeline->InitPipelineLayout(shaderModule->bindings, createInfo.setBindModes, shaderModule->pushConstantBytes);
		pipelineCreateInfo.layout = pipeline->pipelineLayout;
		
		CheckRes(vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline->pipeline));
		
		return WrapPipeline(pipeline);
	}
	
	void ComputePipeline::Bind(CommandContextHandle cc)
	{
		VkCommandBuffer cb = GetCB(cc);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	}
	
	void DispatchCompute(CommandContextHandle cc, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
	{
		VkCommandBuffer cb = GetCB(cc);
		vkCmdDispatch(cb, sizeX, sizeY, sizeZ);
	}
}

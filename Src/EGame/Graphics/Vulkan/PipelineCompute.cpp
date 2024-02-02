#ifndef EG_NO_VULKAN
#include "../../Alloc/ObjectPool.hpp"
#include "Pipeline.hpp"
#include "ShaderModule.hpp"

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

	vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
	vkDestroyPipeline(ctx.device, pipeline, nullptr);

	computePipelinesPool.Delete(this);
}

PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo)
{
	ComputePipeline* pipeline = computePipelinesPool.New();
	pipeline->refCount = 1;
	pipeline->bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	pipeline->pushConstantStages = VK_SHADER_STAGE_COMPUTE_BIT;

	VkComputePipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pipelineCreateInfo.basePipelineIndex = -1;

	pipeline->shaderModule = UnwrapShaderModule(createInfo.computeShader.shaderModule);
	pipeline->shaderModule->ref++;
	InitShaderStageCreateInfo(
		pipelineCreateInfo.stage, pipeline->linearAllocator, createInfo.computeShader, VK_SHADER_STAGE_COMPUTE_BIT);

	pipeline->InitPipelineLayout(
		pipeline->shaderModule->bindings, createInfo.setBindModes, pipeline->shaderModule->pushConstantBytes);
	pipelineCreateInfo.layout = pipeline->pipelineLayout;

	CheckRes(
		vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline->pipeline));

	if (createInfo.label != nullptr)
	{
		SetObjectName(reinterpret_cast<uint64_t>(pipeline->pipeline), VK_OBJECT_TYPE_PIPELINE, createInfo.label);
		SetObjectName(
			reinterpret_cast<uint64_t>(pipeline->pipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, createInfo.label);
	}

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
} // namespace eg::graphics_api::vk

#endif

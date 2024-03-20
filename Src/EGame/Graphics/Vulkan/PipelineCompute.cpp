#ifndef EG_NO_VULKAN
#include "../../Alloc/ObjectPool.hpp"
#include "Buffer.hpp"
#include "Pipeline.hpp"
#include "ShaderModule.hpp"
#include "VulkanCommandContext.hpp"

namespace eg::graphics_api::vk
{
struct ComputePipeline : AbstractPipeline
{
	void Free() override;

	void Bind(CommandContextHandle cc) override;
};

static ConcurrentObjectPool<ComputePipeline> computePipelinesPool;

void ComputePipeline::Free()
{
	AbstractPipeline::Free();
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

	const ShaderModule* shaderModule = UnwrapShaderModule(createInfo.computeShader.shaderModule);
	InitShaderStageCreateInfo(
		pipelineCreateInfo.stage, pipeline->linearAllocator, createInfo.computeShader, VK_SHADER_STAGE_COMPUTE_BIT);

	VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT requiredSubgroupSizeCreateInfo;
	if (ctx.hasSubgroupSizeControlExtension)
	{
		if (createInfo.requireFullSubgroups)
		{
			pipelineCreateInfo.stage.flags |= VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT;
		}

		if (createInfo.requiredSubgroupSize.has_value())
		{
			requiredSubgroupSizeCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
				.requiredSubgroupSize = *createInfo.requiredSubgroupSize,
			};
			PushPNext(pipelineCreateInfo.stage, requiredSubgroupSizeCreateInfo);
		}
		else
		{
			pipelineCreateInfo.stage.flags |= VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT;
		}
	}

	pipeline->InitPipelineLayout(shaderModule->bindings, createInfo.setBindModes, shaderModule->pushConstantBytes);
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

std::optional<uint32_t> GetPipelineSubgroupSize(PipelineHandle pipeline)
{
	if (!ctx.subgroupFeatures.supportsGetPipelineSubgroupSize)
		return std::nullopt;

	const VkPipelineInfoKHR pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR,
		.pipeline = UnwrapPipeline(pipeline)->pipeline,
	};

	uint32_t numExecutables;
	if (vkGetPipelineExecutablePropertiesKHR(ctx.device, &pipelineInfo, &numExecutables, nullptr) != VK_SUCCESS)
		return std::nullopt;
	if (numExecutables == 0)
		return std::nullopt;

	std::vector<VkPipelineExecutablePropertiesKHR> executableProperties(
		numExecutables, { VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR });
	if (vkGetPipelineExecutablePropertiesKHR(ctx.device, &pipelineInfo, &numExecutables, executableProperties.data()) !=
	    VK_SUCCESS)
	{
		return std::nullopt;
	}

	std::optional<uint32_t> subgroupSize;
	for (const VkPipelineExecutablePropertiesKHR& properties : executableProperties)
	{
		if (properties.subgroupSize != 0)
		{
			if (!subgroupSize.has_value())
				subgroupSize = properties.subgroupSize;
			else if (*subgroupSize != properties.subgroupSize)
				return std::nullopt;
		}
	}

	return subgroupSize;
}

void ComputePipeline::Bind(CommandContextHandle cc)
{
	vkCmdBindPipeline(UnwrapCC(cc).cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

void DispatchCompute(CommandContextHandle cc, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	vcc.FlushDescriptorUpdates();
	vkCmdDispatch(vcc.cb, sizeX, sizeY, sizeZ);
}

void DispatchComputeIndirect(CommandContextHandle cc, BufferHandle argsBuffer, uint64_t argsBufferOffset)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	vcc.FlushDescriptorUpdates();
	vkCmdDispatchIndirect(vcc.cb, UnwrapBuffer(argsBuffer)->buffer, argsBufferOffset);
}
} // namespace eg::graphics_api::vk

#endif

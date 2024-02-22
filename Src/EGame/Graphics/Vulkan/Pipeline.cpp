#ifndef EG_NO_VULKAN
#include "Pipeline.hpp"
#include "CachedDescriptorSetLayout.hpp"
#include "ShaderModule.hpp"
#include "VulkanCommandContext.hpp"

namespace eg::graphics_api::vk
{
void DestroyPipeline(PipelineHandle handle)
{
	UnwrapPipeline(handle)->UnRef();
}

void BindPipeline(CommandContextHandle cc, PipelineHandle handle)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	AbstractPipeline* pipeline = UnwrapPipeline(handle);

	vcc.referencedResources.Add(*pipeline);
	vcc.pipeline = pipeline;

	pipeline->Bind(cc);
}

void PushConstants(CommandContextHandle cc, uint32_t offset, uint32_t range, const void* data)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	if (vcc.pipeline == nullptr)
	{
		Log(LogLevel::Error, "gfx", "No pipeline bound when updating push constants.");
		return;
	}

	vkCmdPushConstants(vcc.cb, vcc.pipeline->pipelineLayout, vcc.pipeline->pushConstantStages, offset, range, data);
}

void AbstractPipeline::InitPipelineLayout(
	const DescriptorSetBindings& bindings, const BindMode* setBindModes, uint32_t pushConstantBytes)
{
	// Gets descriptor set layouts for each descriptor set
	uint32_t numDS = 0;
	VkDescriptorSetLayout vkSetLayouts[MAX_DESCRIPTOR_SETS] = {};
	std::fill_n(setLayouts, MAX_DESCRIPTOR_SETS, nullptr);
	for (; numDS < MAX_DESCRIPTOR_SETS && !bindings.sets[numDS].empty(); numDS++)
	{
		setLayouts[numDS] = &CachedDescriptorSetLayout::FindOrCreateNew(bindings.sets[numDS], setBindModes[numDS]);
		vkSetLayouts[numDS] = setLayouts[numDS]->Layout();
	}

	// Creates the pipeline layout
	VkPipelineLayoutCreateInfo layoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutCreateInfo.pSetLayouts = vkSetLayouts;
	layoutCreateInfo.setLayoutCount = numDS;
	VkPushConstantRange pushConstantRange;
	if (pushConstantBytes > 0)
	{
		pushConstantRange = { pushConstantStages, 0, pushConstantBytes };
		layoutCreateInfo.pushConstantRangeCount = 1;
		layoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	}
	CheckRes(vkCreatePipelineLayout(ctx.device, &layoutCreateInfo, nullptr, &pipelineLayout));
}

void InitShaderStageCreateInfo(
	VkPipelineShaderStageCreateInfo& createInfo, LinearAllocator& linAllocator, const ShaderStageInfo& stageInfo,
	VkShaderStageFlagBits stage)
{
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.module = UnwrapShaderModule(stageInfo.shaderModule)->module;
	createInfo.pName = "main";
	createInfo.stage = stage;

	if (!stageInfo.specConstants.empty())
	{
		VkSpecializationInfo* specInfo = linAllocator.New<VkSpecializationInfo>();
		specInfo->dataSize = stageInfo.specConstantsDataSize;
		specInfo->mapEntryCount = UnsignedNarrow<uint32_t>(stageInfo.specConstants.size());

		void* specConstantsData = linAllocator.Allocate(stageInfo.specConstantsDataSize);
		specInfo->pData = specConstantsData;
		std::memcpy(specConstantsData, stageInfo.specConstantsData, stageInfo.specConstantsDataSize);

		auto* mapEntries = linAllocator.AllocateArray<VkSpecializationMapEntry>(stageInfo.specConstants.size());
		specInfo->pMapEntries = mapEntries;
		std::memcpy(mapEntries, stageInfo.specConstants.data(), stageInfo.specConstants.size_bytes());

		createInfo.pSpecializationInfo = specInfo;
	}
	else
	{
		createInfo.pSpecializationInfo = nullptr;
	}
}
} // namespace eg::graphics_api::vk

#endif

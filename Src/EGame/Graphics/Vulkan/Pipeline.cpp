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
	vcc.FlushDescriptorUpdates();

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

void AbstractPipeline::Free()
{
	vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
	vkDestroyPipeline(ctx.device, pipeline, nullptr);
}

void AbstractPipeline::InitPipelineLayout(
	const DescriptorSetBindings& bindings, std::optional<uint32_t> _dynamicDescriptorSetIndex,
	uint32_t pushConstantBytes)
{
	dynamicDescriptorSetIndex = _dynamicDescriptorSetIndex;

	// Gets descriptor set layouts for each descriptor set
	uint32_t numDS = 0;
	VkDescriptorSetLayout vkSetLayouts[MAX_DESCRIPTOR_SETS] = {};
	std::fill_n(setLayouts, MAX_DESCRIPTOR_SETS, nullptr);
	for (; numDS < MAX_DESCRIPTOR_SETS && !bindings.sets[numDS].empty(); numDS++)
	{
		const bool isDynamic = dynamicDescriptorSetIndex == numDS;
		setLayouts[numDS] = &CachedDescriptorSetLayout::FindOrCreateNew(bindings.sets[numDS], isDynamic);
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
		auto specConstantsData = linAllocator.AllocateSpan<uint32_t>(stageInfo.specConstants.size());
		auto mapEntries = linAllocator.AllocateSpan<VkSpecializationMapEntry>(stageInfo.specConstants.size());
		for (size_t i = 0; i < stageInfo.specConstants.size(); i++)
		{
			std::visit(
				[&](auto value) { specConstantsData[i] = std::bit_cast<uint32_t>(value); },
				stageInfo.specConstants[i].value);

			mapEntries[i] = VkSpecializationMapEntry{
				.constantID = stageInfo.specConstants[i].constantID,
				.offset = static_cast<uint32_t>(i * sizeof(uint32_t)),
				.size = sizeof(uint32_t),
			};
		}

		VkSpecializationInfo* specInfo = linAllocator.New<VkSpecializationInfo>();
		specInfo->dataSize = specConstantsData.size_bytes();
		specInfo->pData = specConstantsData.data();
		specInfo->mapEntryCount = UnsignedNarrow<uint32_t>(stageInfo.specConstants.size());
		specInfo->pMapEntries = mapEntries.data();

		createInfo.pSpecializationInfo = specInfo;
	}
	else
	{
		createInfo.pSpecializationInfo = nullptr;
	}
}
} // namespace eg::graphics_api::vk

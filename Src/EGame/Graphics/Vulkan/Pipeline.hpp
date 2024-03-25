#pragma once

#include "../../Alloc/LinearAllocator.hpp"
#include "../SpirvCrossUtils.hpp"
#include "Common.hpp"

namespace eg::graphics_api::vk
{
class CachedDescriptorSetLayout;

struct AbstractPipeline : public Resource
{
	LinearAllocator linearAllocator;

	VkPipeline pipeline;
	VkPipelineBindPoint bindPoint;
	VkShaderStageFlags pushConstantStages;
	VkPipelineLayout pipelineLayout;
	CachedDescriptorSetLayout* setLayouts[MAX_DESCRIPTOR_SETS];
	std::optional<uint32_t> dynamicDescriptorSetIndex;

	void InitPipelineLayout(
		const DescriptorSetBindings& bindings, std::optional<uint32_t> dynamicDescriptorSetIndex,
		uint32_t pushConstantBytes);

	virtual void Bind(CommandContextHandle cc) = 0;

	virtual void Free() override;
};

void InitShaderStageCreateInfo(
	VkPipelineShaderStageCreateInfo& createInfo, LinearAllocator& linAllocator, const ShaderStageInfo& stageInfo,
	VkShaderStageFlagBits stage);

inline AbstractPipeline* UnwrapPipeline(PipelineHandle handle)
{
	return reinterpret_cast<AbstractPipeline*>(handle);
}

inline PipelineHandle WrapPipeline(AbstractPipeline* pipeline)
{
	return reinterpret_cast<PipelineHandle>(pipeline);
}
} // namespace eg::graphics_api::vk

#pragma once

#include "Common.hpp"

namespace eg::graphics_api::vk
{
	struct AbstractPipeline : public Resource
	{
		VkPipelineBindPoint bindPoint;
		VkShaderStageFlags pushConstantStages;
		VkPipelineLayout pipelineLayout;
		size_t setsLayoutIndices[4];
		
		void InitPipelineLayout(const std::vector<VkDescriptorSetLayoutBinding> bindings[MAX_DESCRIPTOR_SETS],
			const BindMode setBindModes[MAX_DESCRIPTOR_SETS], uint32_t pushConstantBytes);
		
		virtual void Bind(CommandContextHandle cc) = 0;
		
		virtual void Free() override = 0;
	};
	
	void InitShaderStageCreateInfo(VkPipelineShaderStageCreateInfo& createInfo, VkShaderModule module,
		VkShaderStageFlagBits stage);
	
	inline AbstractPipeline* UnwrapPipeline(PipelineHandle handle)
	{
		return reinterpret_cast<AbstractPipeline*>(handle);
	}
	
	inline PipelineHandle WrapPipeline(AbstractPipeline* pipeline)
	{
		return reinterpret_cast<PipelineHandle>(pipeline);
	}
}

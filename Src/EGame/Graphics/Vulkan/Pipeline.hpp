#pragma once

#ifndef EG_NO_VULKAN

#include "Common.hpp"
#include "../../Alloc/LinearAllocator.hpp"

namespace eg::graphics_api::vk
{
	class CachedDescriptorSetLayout;
	
	struct AbstractPipeline : public Resource
	{
		LinearAllocator linearAllocator;
		
		VkPipelineBindPoint bindPoint;
		VkShaderStageFlags pushConstantStages;
		VkPipelineLayout pipelineLayout;
		CachedDescriptorSetLayout* setLayouts[MAX_DESCRIPTOR_SETS];
		
		void InitPipelineLayout(const std::vector<VkDescriptorSetLayoutBinding> bindings[MAX_DESCRIPTOR_SETS],
			const BindMode setBindModes[MAX_DESCRIPTOR_SETS], uint32_t pushConstantBytes);
		
		virtual void Bind(CommandContextHandle cc) = 0;
		
		virtual void Free() override = 0;
	};
	
	void InitShaderStageCreateInfo(VkPipelineShaderStageCreateInfo& createInfo, LinearAllocator& linAllocator,
		const ShaderStageInfo& stageInfo, VkShaderStageFlagBits stage);
	
	inline AbstractPipeline* UnwrapPipeline(PipelineHandle handle)
	{
		return reinterpret_cast<AbstractPipeline*>(handle);
	}
	
	inline PipelineHandle WrapPipeline(AbstractPipeline* pipeline)
	{
		return reinterpret_cast<PipelineHandle>(pipeline);
	}
}

#endif

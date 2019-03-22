#pragma once

#include "Common.hpp"

namespace eg::graphics_api::vk
{
	struct ShaderModule
	{
		VkShaderModule module;
		std::atomic_int ref;
		uint32_t pushConstantBytes;
		std::vector<VkDescriptorSetLayoutBinding> bindings[MAX_DESCRIPTOR_SETS];
		
		void UnRef();
	};
	
	inline ShaderModule* UnwrapShaderModule(ShaderModuleHandle handle)
	{
		return reinterpret_cast<ShaderModule*>(handle);
	}
}

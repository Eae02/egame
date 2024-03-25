#pragma once

#include "../SpirvCrossUtils.hpp"
#include "Common.hpp"

namespace eg::graphics_api::vk
{
struct ShaderModule
{
	VkShaderModule module;
	std::atomic_int ref;
	uint32_t pushConstantBytes;

	DescriptorSetBindings bindings;

	std::vector<std::pair<uint32_t, uint32_t>> specConstantIDs;

	void UnRef();
};

inline ShaderModule* UnwrapShaderModule(ShaderModuleHandle handle)
{
	return reinterpret_cast<ShaderModule*>(handle);
}
} // namespace eg::graphics_api::vk

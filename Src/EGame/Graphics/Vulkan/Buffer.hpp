#pragma once

#include "Common.hpp"

namespace eg::graphics_api::vk
{
struct Buffer : Resource
{
	uint64_t size;
	VkBuffer buffer;
	VmaAllocation allocation;
	char* mappedMemory;

	bool autoBarrier;
	BufferUsage currentUsage;
	VkPipelineStageFlags currentStageFlags;

	void CheckUsageState(BufferUsage requiredUsage, const char* actionName);

	void AutoBarrier(
		CommandContextHandle cc, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags = ShaderAccessFlags::None);

	void Free() override;
};

inline Buffer* UnwrapBuffer(BufferHandle handle)
{
	return reinterpret_cast<Buffer*>(handle);
}

void ProcessPendingInitBuffers(bool destroyAll);
} // namespace eg::graphics_api::vk

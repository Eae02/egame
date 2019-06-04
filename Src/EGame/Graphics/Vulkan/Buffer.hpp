#pragma once

#ifndef EG_NO_VULKAN

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
		
		inline void CheckUsageState(BufferUsage requiredUsage, const char* actionName)
		{
			if (autoBarrier && currentUsage != requiredUsage)
			{
				EG_PANIC("Buffer not in the correct usage state when " << actionName << ", did you forget to call UsageHint?");
			}
		}
		
		void AutoBarrier(VkCommandBuffer cb, BufferUsage newUsage,
			ShaderAccessFlags shaderAccessFlags = ShaderAccessFlags::None);
		
		void Free() override;
	};
	
	inline Buffer* UnwrapBuffer(BufferHandle handle)
	{
		return reinterpret_cast<Buffer*>(handle);
	}
}

#endif

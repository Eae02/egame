#ifndef EG_NO_VULKAN
#include "Buffer.hpp"
#include "Pipeline.hpp"
#include "Translation.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::detail
{
	extern uint64_t frameIndex;
}

namespace eg::graphics_api::vk
{
	static ConcurrentObjectPool<Buffer> bufferPool;
	
	void Buffer::Free()
	{
		vmaDestroyBuffer(ctx.allocator, buffer, allocation);
		bufferPool.Delete(this);
	}
	
	struct PendingInitBuffer
	{
		VkBuffer buffer;
		VmaAllocation allocation;
		uint64_t destroyFrame;
	};
	
	static std::vector<PendingInitBuffer> pendingInitBuffers;
	
	void ProcessPendingInitBuffers(bool destroyAll)
	{
		for (int64_t i = pendingInitBuffers.size() - 1; i >= 0; i--)
		{
			if (destroyAll || detail::frameIndex >= pendingInitBuffers[i].destroyFrame)
			{
				vkDestroyBuffer(ctx.device, pendingInitBuffers[i].buffer, nullptr);
				vmaFreeMemory(ctx.allocator, pendingInitBuffers[i].allocation);
				
				pendingInitBuffers[i] = pendingInitBuffers.back();
				pendingInitBuffers.pop_back();
			}
		}
	}
	
	BufferHandle CreateBuffer(const BufferCreateInfo& createInfo)
	{
		Buffer* buffer = bufferPool.New();
		buffer->refCount = 1;
		buffer->size = createInfo.size;
		buffer->autoBarrier = !HasFlag(createInfo.flags, BufferFlags::ManualBarrier);
		buffer->currentUsage = BufferUsage::Undefined;
		buffer->currentStageFlags = 0;
		
		VkBufferCreateInfo vkCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		vkCreateInfo.size = createInfo.size;
		
		if (HasFlag(createInfo.flags, BufferFlags::Update) || HasFlag(createInfo.flags, BufferFlags::CopyDst) ||
		    createInfo.initialData != nullptr)
			vkCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		if (HasFlag(createInfo.flags, BufferFlags::CopySrc))
			vkCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		if (HasFlag(createInfo.flags, BufferFlags::VertexBuffer))
			vkCreateInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (HasFlag(createInfo.flags, BufferFlags::IndexBuffer))
			vkCreateInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (HasFlag(createInfo.flags, BufferFlags::UniformBuffer))
			vkCreateInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (HasFlag(createInfo.flags, BufferFlags::StorageBuffer))
			vkCreateInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		
		const bool wantsMap =
			HasFlag(createInfo.flags, BufferFlags::MapWrite) ||
			HasFlag(createInfo.flags, BufferFlags::MapRead);
		
		VmaAllocationCreateInfo allocationCreateInfo = { };
		
		if (HasFlag(createInfo.flags, BufferFlags::HostAllocate))
		{
			if (HasFlag(createInfo.flags, BufferFlags::Download))
				allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
			else
				allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
		}
		else if (wantsMap)
		{
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		}
		else
		{
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		}
		
		if (wantsMap)
		{
			allocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		
		VmaAllocationInfo allocationInfo;
		CheckRes(vmaCreateBuffer(ctx.allocator, &vkCreateInfo, &allocationCreateInfo, &buffer->buffer,
			&buffer->allocation, &allocationInfo));
		
		if (createInfo.label != nullptr)
		{
			SetObjectName(reinterpret_cast<uint64_t>(buffer->buffer), VK_OBJECT_TYPE_BUFFER, createInfo.label);
		}
		
		buffer->mappedMemory = reinterpret_cast<char*>(allocationInfo.pMappedData);
		
		//Copies initial data
		if (createInfo.initialData != nullptr)
		{
			if (buffer->mappedMemory)
			{
				std::memcpy(buffer->mappedMemory, createInfo.initialData, createInfo.size);
				vmaFlushAllocation(ctx.allocator, buffer->allocation, 0, createInfo.size);
			}
			else
			{
				VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
				barrier.buffer = buffer->buffer;
				barrier.offset = 0;
				barrier.size = VK_WHOLE_SIZE;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				
				VkCommandBuffer cb = GetCB(nullptr);
				vkCmdPipelineBarrier(cb, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
				                     0, 0, nullptr, 1, &barrier, 0, nullptr);
				
				buffer->currentStageFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
				buffer->currentUsage = eg::BufferUsage::CopyDst;
				
				if (createInfo.size <= 65536)
				{
					vkCmdUpdateBuffer(GetCB(nullptr), buffer->buffer, 0, createInfo.size, createInfo.initialData);
				}
				else
				{
					VkBufferCreateInfo initBufferCI = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
					initBufferCI.size = createInfo.size;
					initBufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
					
					VmaAllocationCreateInfo initBufferAllocCI = { };
					initBufferAllocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;
					initBufferAllocCI.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
					
					VkBuffer initBuffer;
					VmaAllocation initAllocation;
					
					VmaAllocationInfo initAllocationInfo;
					CheckRes(vmaCreateBuffer(ctx.allocator, &initBufferCI, &initBufferAllocCI, &initBuffer,
						&initAllocation, &initAllocationInfo));
					
					std::memcpy(initAllocationInfo.pMappedData, createInfo.initialData, createInfo.size);
					vmaFlushAllocation(ctx.allocator, initAllocation, 0, createInfo.size);
					
					VkBufferCopy bufferCopyReg = { 0, 0, createInfo.size };
					vkCmdCopyBuffer(GetCB(nullptr), initBuffer, buffer->buffer, 1, &bufferCopyReg);
					
					pendingInitBuffers.push_back({ initBuffer, initAllocation, detail::frameIndex + MAX_CONCURRENT_FRAMES });
				}
			}
		}
		
		return reinterpret_cast<BufferHandle>(buffer);
	}
	
	void DestroyBuffer(BufferHandle handle)
	{
		UnwrapBuffer(handle)->UnRef();
	}
	
	void* MapBuffer(BufferHandle handle, uint64_t offset, uint64_t range)
	{
		return UnwrapBuffer(handle)->mappedMemory + offset;
	}
	
	void FlushBuffer(BufferHandle handle, uint64_t modOffset, uint64_t modRange)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		vmaFlushAllocation(ctx.allocator, buffer->allocation, modOffset, modRange);
	}
	
	inline VkAccessFlags GetBarrierAccess(BufferUsage usage)
	{
		switch (usage)
		{
		case BufferUsage::Undefined: return 0;
		case BufferUsage::CopySrc: return VK_ACCESS_TRANSFER_READ_BIT;
		case BufferUsage::CopyDst: return VK_ACCESS_TRANSFER_WRITE_BIT;
		case BufferUsage::VertexBuffer: return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		case BufferUsage::IndexBuffer: return VK_ACCESS_INDEX_READ_BIT;
		case BufferUsage::UniformBuffer: return VK_ACCESS_UNIFORM_READ_BIT;
		case BufferUsage::StorageBufferRead: return VK_ACCESS_SHADER_READ_BIT;
		case BufferUsage::StorageBufferWrite: return VK_ACCESS_SHADER_WRITE_BIT;
		case BufferUsage::StorageBufferReadWrite: return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		case BufferUsage::HostRead: return VK_ACCESS_HOST_READ_BIT;
		}
		EG_UNREACHABLE
	}
	
	inline VkPipelineStageFlags GetBarrierStageFlags(BufferUsage usage, ShaderAccessFlags shaderAccessFlags)
	{
		switch (usage)
		{
		case BufferUsage::Undefined: return 0;
		case BufferUsage::CopySrc: return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case BufferUsage::CopyDst: return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case BufferUsage::VertexBuffer: return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		case BufferUsage::IndexBuffer: return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		case BufferUsage::HostRead: return VK_PIPELINE_STAGE_HOST_BIT;
		case BufferUsage::UniformBuffer:
		case BufferUsage::StorageBufferRead:
		case BufferUsage::StorageBufferWrite:
		case BufferUsage::StorageBufferReadWrite:
			return TranslateShaderAccess(shaderAccessFlags);
		}
		EG_UNREACHABLE
	}
	
	void Buffer::AutoBarrier(VkCommandBuffer cb, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags)
	{
		if (!autoBarrier || currentUsage == newUsage)
			return;
		
		VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
		barrier.buffer = buffer;
		barrier.offset = 0;
		barrier.size = VK_WHOLE_SIZE;
		barrier.srcAccessMask = GetBarrierAccess(currentUsage);
		barrier.dstAccessMask = GetBarrierAccess(newUsage);
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		
		VkPipelineStageFlags dstStageFlags = GetBarrierStageFlags(newUsage, shaderAccessFlags);
		if (currentStageFlags == 0)
			currentStageFlags = dstStageFlags;
		
		vkCmdPipelineBarrier(cb, currentStageFlags, dstStageFlags, 0, 0, nullptr, 1, &barrier, 0, nullptr);
		
		currentStageFlags = dstStageFlags;
		currentUsage = newUsage;
	}
	
	void BufferUsageHint(BufferHandle handle, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		RefResource(nullptr, *buffer);
		buffer->AutoBarrier(GetCB(nullptr), newUsage, shaderAccessFlags);
	}
	
	void BufferBarrier(CommandContextHandle cc, BufferHandle handle, const eg::BufferBarrier& barrier)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		RefResource(cc, *buffer);
		
		VkCommandBuffer cb = GetCB(cc);
		
		VkBufferMemoryBarrier vkBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		vkBarrier.buffer = buffer->buffer;
		vkBarrier.offset = barrier.offset;
		vkBarrier.size = barrier.range;
		vkBarrier.srcAccessMask = GetBarrierAccess(barrier.oldUsage);
		vkBarrier.dstAccessMask = GetBarrierAccess(barrier.newUsage);
		vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		
		vkCmdPipelineBarrier(cb, GetBarrierStageFlags(barrier.oldUsage, barrier.oldAccess),
		                     GetBarrierStageFlags(barrier.newUsage, barrier.newAccess), 0, 0, nullptr, 1, &vkBarrier, 0, nullptr);
	}
	
	void FillBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, uint32_t data)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		RefResource(cc, *buffer);
		
		VkCommandBuffer cb = GetCB(cc);
		buffer->AutoBarrier(cb, BufferUsage::CopyDst);
		vkCmdFillBuffer(cb, buffer->buffer, offset, size, data);
	}
	
	void UpdateBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		RefResource(cc, *buffer);
		
		VkCommandBuffer cb = GetCB(cc);
		buffer->AutoBarrier(cb, BufferUsage::CopyDst);
		vkCmdUpdateBuffer(cb, buffer->buffer, offset, size, data);
	}
	
	void CopyBuffer(CommandContextHandle cc, BufferHandle src, BufferHandle dst,
		uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
	{
		Buffer* srcBuffer = UnwrapBuffer(src);
		Buffer* dstBuffer = UnwrapBuffer(dst);
		
		RefResource(cc, *srcBuffer);
		RefResource(cc, *dstBuffer);
		
		VkCommandBuffer cb = GetCB(cc);
		srcBuffer->AutoBarrier(cb, BufferUsage::CopySrc);
		dstBuffer->AutoBarrier(cb, BufferUsage::CopyDst);
		
		const VkBufferCopy copyRegion = { srcOffset, dstOffset, size };
		vkCmdCopyBuffer(cb, srcBuffer->buffer, dstBuffer->buffer, 1, &copyRegion);
	}
	
	void BindVertexBuffer(CommandContextHandle cc, uint32_t binding, BufferHandle bufferHandle, uint32_t offset)
	{
		Buffer* buffer = UnwrapBuffer(bufferHandle);
		RefResource(cc, *buffer);
		
		buffer->CheckUsageState(BufferUsage::VertexBuffer, "binding as a vertex buffer");
		
		VkDeviceSize offsetDS = offset;
		vkCmdBindVertexBuffers(GetCB(cc), binding, 1, &buffer->buffer, &offsetDS);
	}
	
	void BindIndexBuffer(CommandContextHandle cc, IndexType type, BufferHandle bufferHandle, uint32_t offset)
	{
		Buffer* buffer = UnwrapBuffer(bufferHandle);
		RefResource(cc, *buffer);
		
		buffer->CheckUsageState(BufferUsage::IndexBuffer, "binding as an index buffer");
		
		const VkIndexType vkIndexType = type == IndexType::UInt32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
		vkCmdBindIndexBuffer(GetCB(cc), buffer->buffer, offset, vkIndexType);
	}
	
	void BindUniformBuffer(CommandContextHandle cc, BufferHandle bufferHandle, uint32_t set,
		uint32_t binding, uint64_t offset, uint64_t range)
	{
		Buffer* buffer = UnwrapBuffer(bufferHandle);
		RefResource(cc, *buffer);
		
		buffer->CheckUsageState(BufferUsage::UniformBuffer, "binding as a uniform buffer");
		
		AbstractPipeline* pipeline = GetCtxState(cc).pipeline;
		
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = buffer->buffer;
		bufferInfo.offset = offset;
		bufferInfo.range = range;
		
		VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDS.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDS.dstBinding = binding;
		writeDS.dstSet = VK_NULL_HANDLE;
		writeDS.descriptorCount = 1;
		writeDS.pBufferInfo = &bufferInfo;
		
		vkCmdPushDescriptorSetKHR(GetCB(cc), pipeline->bindPoint, pipeline->pipelineLayout, set, 1, &writeDS);
	}
	
	void BindStorageBuffer(CommandContextHandle cc, BufferHandle bufferHandle, uint32_t set,
		uint32_t binding, uint64_t offset, uint64_t range)
	{
		Buffer* buffer = UnwrapBuffer(bufferHandle);
		RefResource(cc, *buffer);
		
		if (buffer->autoBarrier && buffer->currentUsage != BufferUsage::StorageBufferRead && 
			buffer->currentUsage != BufferUsage::StorageBufferWrite &&
			buffer->currentUsage != BufferUsage::StorageBufferReadWrite)
		{
			EG_PANIC("Buffer not in the correct usage state when binding as a storage buffer, did you forget to call UsageHint?");
		}
		
		AbstractPipeline* pipeline = GetCtxState(cc).pipeline;
		
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = buffer->buffer;
		bufferInfo.offset = offset;
		bufferInfo.range = range;
		
		VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDS.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDS.dstBinding = binding;
		writeDS.dstSet = VK_NULL_HANDLE;
		writeDS.descriptorCount = 1;
		writeDS.pBufferInfo = &bufferInfo;
		
		vkCmdPushDescriptorSetKHR(GetCB(cc), pipeline->bindPoint, pipeline->pipelineLayout, set, 1, &writeDS);
	}
}

#endif

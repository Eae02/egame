#include "VulkanBuffer.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::graphics_api::vk
{
	static ConcurrentObjectPool<Buffer> bufferPool;
	
	void Buffer::Free()
	{
		vmaDestroyBuffer(ctx.allocator, buffer, allocation);
		bufferPool.Delete(this);
	}
	
	BufferHandle CreateBuffer(BufferFlags flags, uint64_t size, const void* initialData)
	{
		Buffer* buffer = bufferPool.New();
		buffer->refCount = 1;
		buffer->size = size;
		buffer->autoBarrier = !HasFlag(flags, BufferFlags::ManualBarrier);
		buffer->currentUsage = BufferUsage::Undefined;
		buffer->currentStageFlags = 0;
		
		VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		createInfo.size = size;
		
		if (HasFlag(flags, BufferFlags::Update) || HasFlag(flags, BufferFlags::CopyDst) || initialData != nullptr)
			createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		if (HasFlag(flags, BufferFlags::CopySrc))
			createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		if (HasFlag(flags, BufferFlags::VertexBuffer))
			createInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (HasFlag(flags, BufferFlags::IndexBuffer))
			createInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (HasFlag(flags, BufferFlags::UniformBuffer))
			createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		
		const bool wantsMap = HasFlag(flags, BufferFlags::MapWrite) || HasFlag(flags, BufferFlags::MapRead);
		
		VmaAllocationCreateInfo allocationCreateInfo = { };
		if (HasFlag(flags, BufferFlags::HostAllocate))
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
		else if (wantsMap)
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		else
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		
		VmaAllocationInfo allocationInfo;
		CheckRes(vmaCreateBuffer(ctx.allocator, &createInfo, &allocationCreateInfo, &buffer->buffer,
			&buffer->allocation, &allocationInfo));
		
		buffer->mappedMemory = reinterpret_cast<char*>(allocationInfo.pMappedData);
		
		//TODO: Copy initial data
		
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
	
	void UnmapBuffer(BufferHandle handle, uint64_t modOffset, uint64_t modRange)
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
		case BufferUsage::UniformBuffer:
		{
			VkAccessFlags flags = 0;
			if (HasFlag(shaderAccessFlags, ShaderAccessFlags::Vertex))
				flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			if (HasFlag(shaderAccessFlags, ShaderAccessFlags::Fragment))
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			return flags;
		}
		}
		EG_UNREACHABLE
	}
	
	void Buffer::AutoBarrier(VkCommandBuffer cb, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags)
	{
		if (!autoBarrier || currentUsage == newUsage)
			return;
		
		VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
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
}

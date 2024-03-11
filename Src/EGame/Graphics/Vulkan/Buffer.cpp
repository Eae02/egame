#include <vulkan/vulkan_core.h>
#ifndef EG_NO_VULKAN
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "../Graphics.hpp"
#include "Buffer.hpp"
#include "Pipeline.hpp"
#include "Translation.hpp"
#include "VulkanCommandContext.hpp"

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

void Buffer::CheckUsageState(BufferUsage requiredUsage, const char* actionName)
{
	if (autoBarrier && currentUsage != requiredUsage)
	{
		EG_PANIC("Buffer not in the correct usage state when " << actionName << ", did you forget to call UsageHint?");
	}
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
	if (HasFlag(createInfo.flags, BufferFlags::CopyDst))
		vkCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (HasFlag(createInfo.flags, BufferFlags::VertexBuffer))
		vkCreateInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (HasFlag(createInfo.flags, BufferFlags::IndexBuffer))
		vkCreateInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (HasFlag(createInfo.flags, BufferFlags::UniformBuffer))
		vkCreateInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (HasFlag(createInfo.flags, BufferFlags::StorageBuffer))
		vkCreateInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (HasFlag(createInfo.flags, BufferFlags::IndirectCommands))
		vkCreateInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	const bool wantsMap =
		HasFlag(createInfo.flags, BufferFlags::MapWrite) || HasFlag(createInfo.flags, BufferFlags::MapRead);

	VmaAllocationCreateInfo allocationCreateInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (wantsMap)
	{
		allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		allocationCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}

	VmaAllocationInfo allocationInfo;
	CheckRes(vmaCreateBuffer(
		ctx.allocator, &vkCreateInfo, &allocationCreateInfo, &buffer->buffer, &buffer->allocation, &allocationInfo));

	if (createInfo.label != nullptr)
	{
		SetObjectName(reinterpret_cast<uint64_t>(buffer->buffer), VK_OBJECT_TYPE_BUFFER, createInfo.label);
	}

	buffer->mappedMemory = reinterpret_cast<char*>(allocationInfo.pMappedData);

	// Copies initial data
	if (createInfo.initialData != nullptr)
	{
		if (buffer->mappedMemory)
		{
			std::memcpy(buffer->mappedMemory, createInfo.initialData, createInfo.size);
			vmaFlushAllocation(ctx.allocator, buffer->allocation, 0, createInfo.size);
		}
		else
		{
			VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
			barrier.buffer = buffer->buffer;
			barrier.offset = 0;
			barrier.size = VK_WHOLE_SIZE;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			VkCommandBuffer immediateCB = VulkanCommandContext::currentImmediate->cb;
			vkCmdPipelineBarrier(
				immediateCB, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, 0, 0, nullptr, 1, &barrier, 0,
				nullptr);

			buffer->currentStageFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
			buffer->currentUsage = eg::BufferUsage::CopyDst;

			if (createInfo.size <= 65536)
			{
				vkCmdUpdateBuffer(immediateCB, buffer->buffer, 0, createInfo.size, createInfo.initialData);
			}
			else
			{
				VkBufferCreateInfo initBufferCI = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
				initBufferCI.size = createInfo.size;
				initBufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

				VmaAllocationCreateInfo initBufferAllocCI = {};
				initBufferAllocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;
				initBufferAllocCI.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

				VkBuffer initBuffer;
				VmaAllocation initAllocation;

				VmaAllocationInfo initAllocationInfo;
				CheckRes(vmaCreateBuffer(
					ctx.allocator, &initBufferCI, &initBufferAllocCI, &initBuffer, &initAllocation,
					&initAllocationInfo));

				std::memcpy(initAllocationInfo.pMappedData, createInfo.initialData, createInfo.size);
				vmaFlushAllocation(ctx.allocator, initAllocation, 0, createInfo.size);

				VkBufferCopy bufferCopyReg = { 0, 0, createInfo.size };
				vkCmdCopyBuffer(immediateCB, initBuffer, buffer->buffer, 1, &bufferCopyReg);

				pendingInitBuffers.push_back(
					{ initBuffer, initAllocation, detail::frameIndex + MAX_CONCURRENT_FRAMES });
			}
		}
	}

	return reinterpret_cast<BufferHandle>(buffer);
}

void DestroyBuffer(BufferHandle handle)
{
	UnwrapBuffer(handle)->UnRef();
}

void* MapBuffer(BufferHandle handle, uint64_t offset, std::optional<uint64_t> _range)
{
	return UnwrapBuffer(handle)->mappedMemory + offset;
}

void FlushBuffer(BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange)
{
	Buffer* buffer = UnwrapBuffer(handle);
	uint64_t size = modRange.value_or(buffer->size - modOffset);
	vmaFlushAllocation(ctx.allocator, buffer->allocation, modOffset, size);
}

void InvalidateBuffer(BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange)
{
	Buffer* buffer = UnwrapBuffer(handle);
	uint64_t size = modRange.value_or(buffer->size - modOffset);
	vmaInvalidateAllocation(ctx.allocator, buffer->allocation, modOffset, size);
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
	case BufferUsage::IndirectCommandRead: return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
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
	case BufferUsage::IndirectCommandRead: return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
	case BufferUsage::UniformBuffer:
	case BufferUsage::StorageBufferRead:
	case BufferUsage::StorageBufferWrite:
	case BufferUsage::StorageBufferReadWrite: return TranslateShaderPipelineStage(shaderAccessFlags);
	}
	EG_UNREACHABLE
}

void Buffer::AutoBarrier(CommandContextHandle cc, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags)
{
	if (!autoBarrier || currentUsage == newUsage)
		return;

	if (cc != nullptr)
		EG_PANIC("Vulkan resources used on non-direct contexts must use manual barriers");

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

	vkCmdPipelineBarrier(
		VulkanCommandContext::currentImmediate->cb, currentStageFlags, dstStageFlags, 0, 0, nullptr, 1, &barrier, 0,
		nullptr);

	currentStageFlags = dstStageFlags;
	currentUsage = newUsage;
}

void BufferUsageHint(BufferHandle handle, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags)
{
	Buffer* buffer = UnwrapBuffer(handle);
	VulkanCommandContext::currentImmediate->referencedResources.Add(*buffer);
	buffer->AutoBarrier(nullptr, newUsage, shaderAccessFlags);
}

void BufferBarrier(CommandContextHandle cc, BufferHandle handle, const eg::BufferBarrier& barrier)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* buffer = UnwrapBuffer(handle);
	vcc.referencedResources.Add(*buffer);

	VkBufferMemoryBarrier vkBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
	vkBarrier.buffer = buffer->buffer;
	vkBarrier.offset = barrier.offset;
	vkBarrier.size = barrier.range.value_or(VK_WHOLE_SIZE);
	vkBarrier.srcAccessMask = GetBarrierAccess(barrier.oldUsage);
	vkBarrier.dstAccessMask = GetBarrierAccess(barrier.newUsage);
	vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	vkCmdPipelineBarrier(
		vcc.cb, GetBarrierStageFlags(barrier.oldUsage, barrier.oldAccess),
		GetBarrierStageFlags(barrier.newUsage, barrier.newAccess), 0, 0, nullptr, 1, &vkBarrier, 0, nullptr);
}

void FillBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, uint8_t data)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* buffer = UnwrapBuffer(handle);
	vcc.referencedResources.Add(*buffer);

	// Repeats data 4 times because the vulkan function takes a 32 bit int
	const uint32_t data16 = static_cast<uint32_t>(data) | (static_cast<uint32_t>(data) << 8);
	const uint32_t data32 = data16 | (data16 << 16);

	buffer->AutoBarrier(cc, BufferUsage::CopyDst);
	vkCmdFillBuffer(vcc.cb, buffer->buffer, offset, size, data32);
}

void UpdateBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* buffer = UnwrapBuffer(handle);
	vcc.referencedResources.Add(*buffer);

	buffer->AutoBarrier(cc, BufferUsage::CopyDst);
	vkCmdUpdateBuffer(vcc.cb, buffer->buffer, offset, size, data);
}

void CopyBuffer(
	CommandContextHandle cc, BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);

	Buffer* srcBuffer = UnwrapBuffer(src);
	Buffer* dstBuffer = UnwrapBuffer(dst);

	vcc.referencedResources.Add(*srcBuffer);
	vcc.referencedResources.Add(*dstBuffer);

	srcBuffer->AutoBarrier(cc, BufferUsage::CopySrc);
	dstBuffer->AutoBarrier(cc, BufferUsage::CopyDst);

	const VkBufferCopy copyRegion = { srcOffset, dstOffset, size };
	vkCmdCopyBuffer(vcc.cb, srcBuffer->buffer, dstBuffer->buffer, 1, &copyRegion);
}

void BindVertexBuffer(CommandContextHandle cc, uint32_t binding, BufferHandle bufferHandle, uint32_t offset)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* buffer = UnwrapBuffer(bufferHandle);

	EG_ASSERT(!buffer->autoBarrier || cc == nullptr);

	vcc.referencedResources.Add(*buffer);

	buffer->CheckUsageState(BufferUsage::VertexBuffer, "binding as a vertex buffer");

	VkDeviceSize offsetDS = offset;
	vkCmdBindVertexBuffers(vcc.cb, binding, 1, &buffer->buffer, &offsetDS);
}

void BindIndexBuffer(CommandContextHandle cc, IndexType type, BufferHandle bufferHandle, uint32_t offset)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* buffer = UnwrapBuffer(bufferHandle);

	EG_ASSERT(!buffer->autoBarrier || cc == nullptr);

	vcc.referencedResources.Add(*buffer);

	buffer->CheckUsageState(BufferUsage::IndexBuffer, "binding as an index buffer");

	const VkIndexType vkIndexType = type == IndexType::UInt32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
	vkCmdBindIndexBuffer(vcc.cb, buffer->buffer, offset, vkIndexType);
}
} // namespace eg::graphics_api::vk

#endif

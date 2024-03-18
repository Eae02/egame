#include "WGPUBuffer.hpp"
#include "EGame/Alloc/ObjectPool.hpp"
#include "WGPUCommandContext.hpp"

namespace eg::graphics_api::webgpu
{
static ConcurrentObjectPool<Buffer> bufferPool;

BufferHandle CreateBuffer(const BufferCreateInfo& createInfo)
{
	WGPUBufferUsageFlags usageFlags = 0;
	if (HasFlag(createInfo.flags, BufferFlags::MapWrite))
		usageFlags |= WGPUBufferUsage_MapWrite;
	if (HasFlag(createInfo.flags, BufferFlags::MapRead))
		usageFlags |= WGPUBufferUsage_MapRead;
	if (HasFlag(createInfo.flags, BufferFlags::CopySrc))
		usageFlags |= WGPUBufferUsage_CopySrc;
	if (HasFlag(createInfo.flags, BufferFlags::CopyDst) || HasFlag(createInfo.flags, BufferFlags::Update))
		usageFlags |= WGPUBufferUsage_CopyDst;
	if (HasFlag(createInfo.flags, BufferFlags::VertexBuffer))
		usageFlags |= WGPUBufferUsage_Vertex;
	if (HasFlag(createInfo.flags, BufferFlags::IndexBuffer))
		usageFlags |= WGPUBufferUsage_Index;
	if (HasFlag(createInfo.flags, BufferFlags::UniformBuffer))
		usageFlags |= WGPUBufferUsage_Uniform;
	if (HasFlag(createInfo.flags, BufferFlags::StorageBuffer))
		usageFlags |= WGPUBufferUsage_Storage;
	if (HasFlag(createInfo.flags, BufferFlags::IndirectCommands))
		usageFlags |= WGPUBufferUsage_Indirect;

	const bool wantsMap =
		HasFlag(createInfo.flags, BufferFlags::MapWrite) || HasFlag(createInfo.flags, BufferFlags::MapRead);

	const WGPUBufferDescriptor bufferDesc = {
		.label = createInfo.label,
		.size = createInfo.size,
		.usage = usageFlags,
		.mappedAtCreation = createInfo.initialData != nullptr || wantsMap,
	};

	Buffer* buffer = bufferPool.New();
	buffer->buffer = wgpuDeviceCreateBuffer(wgpuctx.device, &bufferDesc);
	buffer->size = createInfo.size;

	if (createInfo.initialData != nullptr)
	{
		void* mappedMemory = wgpuBufferGetMappedRange(buffer->buffer, 0, createInfo.size);
		std::memcpy(mappedMemory, createInfo.initialData, createInfo.size);
		wgpuBufferUnmap(buffer->buffer);
	}

	if (wantsMap)
	{
		buffer->allocationForPersistentMap = std::make_unique<char[]>(createInfo.size);
		if (createInfo.initialData != nullptr)
			std::memcpy(buffer->allocationForPersistentMap.get(), createInfo.initialData, createInfo.size);
	}

	return reinterpret_cast<BufferHandle>(buffer);
}

void DestroyBuffer(BufferHandle handle)
{
	Buffer& buffer = Buffer::Unwrap(handle);
	wgpuBufferDestroy(buffer.buffer);
	bufferPool.Delete(&buffer);
}

void BufferUsageHint(BufferHandle handle, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags) {}
void BufferBarrier(CommandContextHandle ctx, BufferHandle handle, const eg::BufferBarrier& barrier) {}

void* MapBuffer(BufferHandle handle, uint64_t offset, std::optional<uint64_t> _range)
{
	return Buffer::Unwrap(handle).allocationForPersistentMap.get() + offset;
}

void FlushBuffer(BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange)
{
	Buffer& buffer = Buffer::Unwrap(handle);
	uint64_t actualRange = modRange.value_or(buffer.size - modOffset);
	void* memory = wgpuBufferGetMappedRange(buffer.buffer, modOffset, actualRange);
	EG_ASSERT(memory != nullptr);
	std::memcpy(memory, buffer.allocationForPersistentMap.get() + modOffset, actualRange);
	wgpuBufferUnmap(buffer.buffer);
}

void InvalidateBuffer(BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange)
{
	Buffer& buffer = Buffer::Unwrap(handle);
	uint64_t actualRange = modRange.value_or(buffer.size - modOffset);
	void* memory = wgpuBufferGetMappedRange(buffer.buffer, modOffset, actualRange);
	EG_ASSERT(memory != nullptr);
	std::memcpy(buffer.allocationForPersistentMap.get() + modOffset, memory, actualRange);
	wgpuBufferUnmap(buffer.buffer);
}

void UpdateBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);
	wgpuCommandEncoderWriteBuffer(
		wcc.encoder, Buffer::Unwrap(handle).buffer, offset, static_cast<const uint8_t*>(data), size);
}

void FillBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, uint8_t data)
{
	EG_PANIC("Unimplemented: FillBuffer")
}

void CopyBuffer(
	CommandContextHandle cc, BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);
	wgpuCommandEncoderCopyBufferToBuffer(
		wcc.encoder, Buffer::Unwrap(src).buffer, srcOffset, Buffer::Unwrap(dst).buffer, dstOffset, size);
}

void BindUniformBuffer(
	CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	EG_PANIC("WebGPU Not Available: BindUniformBuffer")
}

void BindStorageBuffer(
	CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	EG_PANIC("WebGPU Not Available: BindStorageBuffer")
}
} // namespace eg::graphics_api::webgpu

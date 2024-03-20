#include "WGPUBuffer.hpp"
#include "EGame/Alloc/ObjectPool.hpp"
#include "WGPUCommandContext.hpp"

namespace eg::graphics_api::webgpu
{
static ConcurrentObjectPool<Buffer> bufferPool;

BufferHandle CreateBuffer(const BufferCreateInfo& createInfo)
{
	WGPUBufferUsageFlags usageFlags = 0;
	if (HasFlag(createInfo.flags, BufferFlags::CopySrc) || HasFlag(createInfo.flags, BufferFlags::MapRead))
		usageFlags |= WGPUBufferUsage_CopySrc;
	if (HasFlag(createInfo.flags, BufferFlags::CopyDst) || HasFlag(createInfo.flags, BufferFlags::Update) ||
	    HasFlag(createInfo.flags, BufferFlags::MapWrite))
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

	const WGPUBufferDescriptor bufferDesc = {
		.label = createInfo.label,
		.size = createInfo.size,
		.usage = usageFlags,
		.mappedAtCreation = createInfo.initialData != nullptr,
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

	if (HasFlag(createInfo.flags, BufferFlags::MapWrite) || HasFlag(createInfo.flags, BufferFlags::MapRead))
	{
		buffer->mapData = BufferMapData::Alloc(createInfo.size);
		if (createInfo.initialData != nullptr)
			std::memcpy(buffer->mapData->memory.data(), createInfo.initialData, createInfo.size);
	}

	if (HasFlag(createInfo.flags, BufferFlags::MapRead))
	{
		const WGPUBufferDescriptor readbackBufferDesc = {
			.label = createInfo.label,
			.size = createInfo.size,
			.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
		};
		buffer->mapData->readbackBuffer = wgpuDeviceCreateBuffer(wgpuctx.device, &readbackBufferDesc);
	}

	return reinterpret_cast<BufferHandle>(buffer);
}

void BufferMapData::Deref()
{
	if (--refcount <= 0)
	{
		if (readbackBuffer != nullptr)
			wgpuBufferDestroy(readbackBuffer);
		free(this);
	}
}

BufferMapData* BufferMapData::Alloc(uint64_t size)
{
	void* memory = malloc(sizeof(BufferMapData) + size);
	BufferMapData* mapData = new (memory) BufferMapData;
	mapData->memory = { static_cast<char*>(memory) + sizeof(BufferMapData), size };
	return mapData;
}

void DestroyBuffer(BufferHandle handle)
{
	Buffer& buffer = Buffer::Unwrap(handle);
	buffer.mapData->Deref();
	wgpuBufferDestroy(buffer.buffer);
	bufferPool.Delete(&buffer);
}

static void SetBufferUsage(CommandContext& cc, BufferHandle handle, BufferUsage newUsage)
{
	Buffer& buffer = Buffer::Unwrap(handle);
	if (newUsage == BufferUsage::HostRead && buffer.mapData != nullptr && buffer.mapData->readbackBuffer != nullptr)
	{
		// wgpuCommandEncoderCopyBufferToBuffer(
		// 	cc.encoder, buffer.buffer, 0, buffer.mapData->readbackBuffer, 0, buffer.size);
		// buffer.mapData->refcount++;
		// cc.readbackBuffers.push_back(buffer.mapData);
	}
}

void BufferUsageHint(BufferHandle handle, BufferUsage newUsage, ShaderAccessFlags _shaderAccessFlags)
{
	SetBufferUsage(CommandContext::main, handle, newUsage);
}

void BufferBarrier(CommandContextHandle ctx, BufferHandle handle, const eg::BufferBarrier& barrier)
{
	SetBufferUsage(CommandContext::Unwrap(ctx), handle, barrier.newUsage);
}

void* MapBuffer(BufferHandle handle, uint64_t offset, std::optional<uint64_t> _range)
{
	return Buffer::Unwrap(handle).mapData->memory.data() + offset;
}

void FlushBuffer(BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange)
{
	Buffer& buffer = Buffer::Unwrap(handle);
	uint64_t actualRange = modRange.value_or(buffer.size - modOffset);

	wgpuQueueWriteBuffer(
		wgpuctx.queue, buffer.buffer, modOffset, buffer.mapData->memory.data() + modOffset, actualRange);
}

void InvalidateBuffer(BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange) {}

void UpdateBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);
	wgpuCommandEncoderWriteBuffer(
		wcc.encoder, Buffer::Unwrap(handle).buffer, offset, static_cast<const uint8_t*>(data), size);
}

void FillBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, uint8_t data)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);
	WGPUBuffer buffer = Buffer::Unwrap(handle).buffer;
	if (data == 0)
	{
		wgpuCommandEncoderClearBuffer(wcc.encoder, buffer, offset, size);
	}
	else
	{
		std::unique_ptr<uint8_t[]> dataAllocation = std::make_unique<uint8_t[]>(size);
		std::fill_n(dataAllocation.get(), size, data);
		wgpuCommandEncoderWriteBuffer(wcc.encoder, buffer, offset, dataAllocation.get(), size);
	}
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

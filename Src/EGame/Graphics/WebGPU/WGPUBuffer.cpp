#include "WGPUBuffer.hpp"
#include "EGame/Alloc/ObjectPool.hpp"
#include "WGPUCommandContext.hpp"

#include <memory>

namespace eg::graphics_api::webgpu
{
static ConcurrentObjectPool<Buffer> bufferPool;

void Buffer::Deref()
{
	if ((--refCount) == 0)
	{
		OnFrameEnd(
			[b = buffer, rb = readbackBuffer]
			{
				wgpuBufferDestroy(b);
				if (rb != nullptr)
					wgpuBufferDestroy(rb);
			});
		bufferPool.Delete(this);
	}
}

BufferHandle CreateBuffer(const BufferCreateInfo& createInfo)
{
	WGPUBufferUsageFlags usageFlags = 0;
	if (HasFlag(createInfo.flags, BufferFlags::CopySrc))
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

	size_t paddedSize = createInfo.size;
	if (HasFlag(createInfo.flags, BufferFlags::UniformBuffer))
		paddedSize = RoundToNextMultiple(paddedSize, 16);

	Buffer* buffer = bufferPool.New();

	if (HasFlag(createInfo.flags, BufferFlags::MapRead))
	{
		usageFlags |= WGPUBufferUsage_CopySrc;

		const WGPUBufferDescriptor readbackBufferDesc = {
			.label = createInfo.label,
			.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
			.size = paddedSize,
		};
		buffer->readbackBuffer = wgpuDeviceCreateBuffer(wgpuctx.device, &readbackBufferDesc);
	}

	const WGPUBufferDescriptor bufferDesc = {
		.label = createInfo.label,
		.usage = usageFlags,
		.size = paddedSize,
		.mappedAtCreation = createInfo.initialData != nullptr,
	};

	buffer->buffer = wgpuDeviceCreateBuffer(wgpuctx.device, &bufferDesc);
	buffer->size = paddedSize;

	if (createInfo.initialData != nullptr)
	{
		void* mappedMemory = wgpuBufferGetMappedRange(buffer->buffer, 0, createInfo.size);
		std::memcpy(mappedMemory, createInfo.initialData, createInfo.size);
		wgpuBufferUnmap(buffer->buffer);
	}

	if (HasFlag(createInfo.flags, BufferFlags::MapWrite) || HasFlag(createInfo.flags, BufferFlags::MapRead))
	{
		buffer->mapMemory = std::make_unique<char[]>(createInfo.size);
		if (createInfo.initialData != nullptr)
			std::memcpy(buffer->mapMemory.get(), createInfo.initialData, createInfo.size);
	}

	return reinterpret_cast<BufferHandle>(buffer);
}

void DestroyBuffer(BufferHandle handle)
{
	Buffer::Unwrap(handle).Deref();
}

static void SetBufferUsage(CommandContext& cc, BufferHandle handle, BufferUsage newUsage)
{
	Buffer& buffer = Buffer::Unwrap(handle);
	if (newUsage == BufferUsage::HostRead && buffer.readbackBuffer != nullptr)
	{
		if (!buffer.pendingReadback.exchange(true))
		{
			cc.EndComputePass();
			wgpuCommandEncoderCopyBufferToBuffer(cc.encoder, buffer.buffer, 0, buffer.readbackBuffer, 0, buffer.size);
			cc.AddReadbackBuffer(buffer);
		}
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
	return Buffer::Unwrap(handle).mapMemory.get() + offset;
}

void FlushBuffer(BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange)
{
	Buffer& buffer = Buffer::Unwrap(handle);

	constexpr uint64_t ALIGNMENT = 4;

	uint64_t endOffset = modRange.has_value() ? (modOffset + *modRange) : buffer.size;
	endOffset = (endOffset + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

	uint64_t startOffset = modOffset & ~(ALIGNMENT - 1);

	wgpuQueueWriteBuffer(
		wgpuctx.queue, buffer.buffer, startOffset, buffer.mapMemory.get() + startOffset, endOffset - startOffset);
}

void InvalidateBuffer(BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange) {}

void UpdateBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);
	wcc.EndComputePass();

	wgpuCommandEncoderWriteBuffer(
		wcc.encoder, Buffer::Unwrap(handle).buffer, offset, static_cast<const uint8_t*>(data), size);
}

void FillBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, uint8_t data)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);
	wcc.EndComputePass();

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
	wcc.EndComputePass();

	wgpuCommandEncoderCopyBufferToBuffer(
		wcc.encoder, Buffer::Unwrap(src).buffer, srcOffset, Buffer::Unwrap(dst).buffer, dstOffset, size);
}

void BindUniformBuffer(
	CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	EG_PANIC("Unsupported: BindUniformBuffer")
}

void BindStorageBuffer(
	CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	EG_PANIC("Unsupported: BindStorageBuffer")
}
} // namespace eg::graphics_api::webgpu

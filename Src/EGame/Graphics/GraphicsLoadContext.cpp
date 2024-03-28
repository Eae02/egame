#include "GraphicsLoadContext.hpp"

namespace eg
{
GraphicsLoadContext GraphicsLoadContext::Direct;

void GraphicsLoadContext::StagingBuffer::Flush(uint64_t offset, std::optional<uint64_t> size) const
{
	if (needsFlush)
	{
		uint64_t realSize = size.value_or(memory.size() - offset);
		gal::FlushBuffer(buffer.handle, bufferOffset + offset, realSize);
	}
}

void GraphicsLoadContext::InitStagingBuffers(std::optional<uint64_t> stagingBufferSize)
{
	if (stagingBufferSize.has_value())
	{
		Buffer buffer(
			BufferFlags::MapWrite | BufferFlags::CopySrc | BufferFlags::ManualBarrier, *stagingBufferSize, nullptr);
		void* bufferMemory = buffer.Map();

		m_stagingBuffers = SingleStagingBuffer{
			.buffer = std::move(buffer),
			.size = *stagingBufferSize,
			.offset = 0,
			.memory = bufferMemory,
		};
	}
	else
	{
		m_stagingBuffers = std::vector<Buffer>();
	}
}

GraphicsLoadContext GraphicsLoadContext::CreateDeferred(std::optional<uint64_t> stagingBufferSize)
{
	GraphicsLoadContext loadContext;

	loadContext.InitStagingBuffers(stagingBufferSize);

	if (HasFlag(GetGraphicsDeviceInfo().features, DeviceFeatureFlags::DeferredContext))
	{
		loadContext.m_ownedCommandContext = CommandContext::CreateDeferred(Queue::Main);
		loadContext.m_ownedCommandContext->BeginRecording(CommandContextBeginFlags::OneTimeSubmit);
		loadContext.m_mode = Mode::DeferredContext;
	}
	else
	{
		loadContext.m_mode = Mode::DeferToGraphicsThread;
	}

	return loadContext;
}

GraphicsLoadContext GraphicsLoadContext::CreateWrapping(
	CommandContext& commandContext, std::optional<uint64_t> stagingBufferSize)
{
	GraphicsLoadContext loadContext;

	loadContext.InitStagingBuffers(stagingBufferSize);

	loadContext.m_commandContext = &commandContext;
	loadContext.m_mode = Mode::DeferredContext;

	return loadContext;
}

FenceHandle GraphicsLoadContext::FinishDeferred()
{
	if (SingleStagingBuffer* stagingBuffer = std::get_if<SingleStagingBuffer>(&m_stagingBuffers))
	{
		stagingBuffer->buffer.Flush(0, stagingBuffer->offset);
	}

	if (m_mode == Mode::DeferredContext && m_ownedCommandContext.has_value())
	{
		FenceHandle fence = gal::CreateFence();

		m_ownedCommandContext->FinishRecording();
		m_ownedCommandContext->Submit(CommandContextSubmitArgs{ .fence = fence });

		return fence;
	}

	if (m_mode == Mode::DeferToGraphicsThread)
	{
		for (const auto& func : m_onGraphicsThreadCallbacks)
			func(DC);
		m_onGraphicsThreadCallbacks.clear();
	}

	return nullptr;
}

GraphicsLoadContext::StagingBuffer GraphicsLoadContext::AllocateStagingBuffer(uint64_t size)
{
	if (std::holds_alternative<std::monostate>(m_stagingBuffers))
	{
		UploadBuffer buffer = GetTemporaryUploadBuffer(size);
		return StagingBuffer{
			.memory = std::span<char>(static_cast<char*>(buffer.Map()), size),
			.buffer = buffer.buffer,
			.bufferOffset = buffer.offset,
			.needsFlush = true,
		};
	}

	if (auto* buffers = std::get_if<std::vector<Buffer>>(&m_stagingBuffers))
	{
		buffers->emplace_back(BufferFlags::CopySrc | BufferFlags::MapWrite | BufferFlags::ManualBarrier, size, nullptr);
		return StagingBuffer{
			.memory = std::span<char>(static_cast<char*>(buffers->back().Map()), size),
			.buffer = buffers->back(),
			.bufferOffset = 0,
			.needsFlush = true,
		};
	}

	SingleStagingBuffer& singleStagingBuffer = std::get<SingleStagingBuffer>(m_stagingBuffers);

	singleStagingBuffer.offset = RoundToNextMultiple(singleStagingBuffer.offset, static_cast<uint64_t>(16));
	uint64_t offset = singleStagingBuffer.offset;

	singleStagingBuffer.offset += size;
	EG_ASSERT(singleStagingBuffer.offset <= singleStagingBuffer.size);

	return StagingBuffer{
		.memory = std::span<char>(static_cast<char*>(singleStagingBuffer.memory) + offset, size),
		.buffer = singleStagingBuffer.buffer,
		.bufferOffset = offset,
		.needsFlush = false,
	};
}
} // namespace eg

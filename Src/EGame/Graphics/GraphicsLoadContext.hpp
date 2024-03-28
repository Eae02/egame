#pragma once

#include <span>

#include "AbstractionHL.hpp"

namespace eg
{
class EG_API GraphicsLoadContext
{
public:
	struct StagingBuffer
	{
		std::span<char> memory;
		BufferRef buffer;
		uint64_t bufferOffset;
		bool needsFlush;

		void Flush(uint64_t offset = 0, std::optional<uint64_t> size = std::nullopt) const;
	};

	static GraphicsLoadContext CreateDeferred(std::optional<uint64_t> stagingBufferSize);

	static GraphicsLoadContext CreateWrapping(
		CommandContext& commandContext, std::optional<uint64_t> stagingBufferSize);

	static GraphicsLoadContext Direct;

	StagingBuffer AllocateStagingBuffer(uint64_t size);

	FenceHandle FinishDeferred();

	template <typename F>
	void OnGraphicsThread(F func)
	{
		if (m_mode == Mode::DeferToGraphicsThread)
			m_onGraphicsThreadCallbacks.emplace_back(std::move(func));
		else
			func(GetCommandContext());
	}

	CommandContext& GetCommandContext()
	{
		if (m_ownedCommandContext.has_value())
			return *m_ownedCommandContext;
		if (m_commandContext != nullptr)
			return *m_commandContext;
		return DC;
	}

private:
	GraphicsLoadContext() = default;

	void InitStagingBuffers(std::optional<uint64_t> stagingBufferSize);

	struct DefferedContext
	{
		CommandContext context;
	};

	enum class Mode
	{
		Direct,
		DeferredContext,
		DeferToGraphicsThread,
	};

	Mode m_mode = Mode::Direct;

	std::optional<CommandContext> m_ownedCommandContext;
	CommandContext* m_commandContext = nullptr;

	std::vector<std::function<void(CommandContext&)>> m_onGraphicsThreadCallbacks;

	struct SingleStagingBuffer
	{
		Buffer buffer;
		uint64_t size;
		uint64_t offset;
		void* memory;
	};

	std::variant<std::monostate, std::vector<Buffer>, SingleStagingBuffer> m_stagingBuffers;
};
} // namespace eg

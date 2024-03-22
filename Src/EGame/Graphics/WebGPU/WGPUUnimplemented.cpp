#include "../Abstraction.hpp"

// clang-format off

namespace eg::graphics_api::webgpu
{
CommandContextHandle CreateCommandContext(Queue queue) { EG_PANIC("Unimplemented: CreateCommandContext") }
void DestroyCommandContext(CommandContextHandle context) { EG_PANIC("Unimplemented: DestroyCommandContext") }
void BeginRecordingCommandContext(CommandContextHandle context, CommandContextBeginFlags flags) { EG_PANIC("Unimplemented: BeginRecordingCommandContext") }
void FinishRecordingCommandContext(CommandContextHandle context) { EG_PANIC("Unimplemented: FinishRecordingCommandContext") }
void SubmitCommandContext(CommandContextHandle context, const CommandContextSubmitArgs& args){ EG_PANIC("Unimplemented: SubmitCommandContext") }

FenceHandle CreateFence() { EG_PANIC("Unimplemented: CreateFence") }
void DestroyFence(FenceHandle handle) { EG_PANIC("Unimplemented: DestroyFence") }
FenceStatus WaitForFence(FenceHandle handle, uint64_t timeout) { EG_PANIC("Unimplemented: WaitForFence") }

void PushConstants(CommandContextHandle cc, uint32_t offset, uint32_t range, const void* data) { EG_PANIC("Unimplemented: PushConstants") }

QueryPoolHandle CreateQueryPool(QueryType type, uint32_t queryCount) { return nullptr; }
void DestroyQueryPool(QueryPoolHandle queryPool) { }
bool GetQueryResults(QueryPoolHandle queryPool, uint32_t firstQuery, uint32_t numQueries, uint64_t dataSize, void* data)
{
	std::memset(data, 0, dataSize);
	return true;
}
void CopyQueryResults(CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries, BufferHandle dstBufferHandle, uint64_t dstOffset) { }
void WriteTimestamp(CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t query) { }
void ResetQueries(CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries) { }
void BeginQuery(CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t query) { }
void EndQuery(CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t query) { }
} // namespace eg::graphics_api::webgpu

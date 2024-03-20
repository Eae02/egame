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

FramebufferHandle CreateFramebuffer(const FramebufferCreateInfo& createInfo) { EG_PANIC("Unimplemented: CreateFramebuffer") }
void DestroyFramebuffer(FramebufferHandle framebuffer) { EG_PANIC("Unimplemented: DestroyFramebuffer") }

PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo) { EG_PANIC("Unimplemented: CreateComputePipeline") }
void PushConstants(CommandContextHandle cc, uint32_t offset, uint32_t range, const void* data) { EG_PANIC("Unimplemented: PushConstants") }

void DispatchCompute(CommandContextHandle cc, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ) { EG_PANIC("Unimplemented: DispatchCompute") }
void DispatchComputeIndirect(CommandContextHandle cc, BufferHandle argsBuffer, uint64_t argsBufferOffset) { EG_PANIC("Unimplemented: DispatchComputeIndirect") }

void SetViewport(CommandContextHandle cc, float x, float y, float w, float h) { EG_PANIC("Unimplemented: SetViewport") }
void SetScissor(CommandContextHandle cc, int x, int y, int w, int h) { EG_PANIC("Unimplemented: SetScissor") }
void SetStencilValue(CommandContextHandle cc, StencilValue kind, uint32_t val) { EG_PANIC("Unimplemented: SetStencilValue") }
void SetWireframe(CommandContextHandle cc, bool wireframe) { EG_PANIC("Unimplemented: SetWireframe") }
void SetCullMode(CommandContextHandle cc, CullMode cullMode) { EG_PANIC("Unimplemented: SetCullMode") }

void BindIndexBuffer(CommandContextHandle cc, IndexType type, BufferHandle buffer, uint32_t offset) { EG_PANIC("Unimplemented: BindIndexBuffer") }
void BindVertexBuffer(CommandContextHandle cc, uint32_t binding, BufferHandle buffer, uint32_t offset) { EG_PANIC("Unimplemented: BindVertexBuffer") }

void Draw(CommandContextHandle cc, uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances) { EG_PANIC("Unimplemented: Draw") }
void DrawIndexed(CommandContextHandle cc, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance, uint32_t numInstances) { EG_PANIC("Unimplemented: DrawIndexed") }

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

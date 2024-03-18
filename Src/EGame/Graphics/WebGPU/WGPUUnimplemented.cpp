#include "../Abstraction.hpp"

namespace eg::graphics_api::webgpu
{
CommandContextHandle CreateCommandContext(Queue queue)
{
	EG_PANIC("Unimplemented: CreateCommandContext")
}
void DestroyCommandContext(CommandContextHandle context)
{
	EG_PANIC("Unimplemented: DestroyCommandContext")
}
void BeginRecordingCommandContext(CommandContextHandle context, CommandContextBeginFlags flags)
{
	EG_PANIC("Unimplemented: BeginRecordingCommandContext")
}
void FinishRecordingCommandContext(CommandContextHandle context)
{
	EG_PANIC("Unimplemented: FinishRecordingCommandContext")
}
void SubmitCommandContext(CommandContextHandle context, const CommandContextSubmitArgs& args){
	EG_PANIC("Unimplemented: SubmitCommandContext")
}

FenceHandle CreateFence()
{
	EG_PANIC("Unimplemented: CreateFence")
}
void DestroyFence(FenceHandle handle){ EG_PANIC("Unimplemented: DestroyFence") } FenceStatus
	WaitForFence(FenceHandle handle, uint64_t timeout){ EG_PANIC("Unimplemented: WaitForFence") }

DescriptorSetHandle CreateDescriptorSetP(PipelineHandle pipeline, uint32_t set){
	EG_PANIC("Unimplemented: CreateDescriptorSetP")
} DescriptorSetHandle CreateDescriptorSetB(std::span<const DescriptorSetBinding> bindings)
{
	EG_PANIC("Unimplemented: CreateDescriptorSetB")
}
void DestroyDescriptorSet(DescriptorSetHandle set)
{
	EG_PANIC("Unimplemented: DestroyDescriptorSet")
}
void BindTextureDS(TextureViewHandle textureView, SamplerHandle sampler, DescriptorSetHandle set, uint32_t binding)
{
	EG_PANIC("Unimplemented: BindTextureDS")
}
void BindStorageImageDS(TextureViewHandle textureView, DescriptorSetHandle set, uint32_t binding)
{
	EG_PANIC("Unimplemented: BindStorageImageDS")
}
void BindUniformBufferDS(
	BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> range)
{
	EG_PANIC("Unimplemented: BindUniformBufferDS")
}
void BindStorageBufferDS(
	BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> range)
{
	EG_PANIC("Unimplemented: BindStorageBufferDS")
}
void BindDescriptorSet(
	CommandContextHandle ctx, uint32_t set, DescriptorSetHandle handle,
	std::span<const uint32_t> dynamicOffsets){ EG_PANIC("Unimplemented: BindDescriptorSet") }

FramebufferHandle CreateFramebuffer(const FramebufferCreateInfo& createInfo)
{
	EG_PANIC("Unimplemented: CreateFramebuffer")
}
void DestroyFramebuffer(FramebufferHandle framebuffer){ EG_PANIC("Unimplemented: DestroyFramebuffer") }

PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo){
	EG_PANIC("Unimplemented: CreateGraphicsPipeline")
} PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo){
	EG_PANIC("Unimplemented: CreateComputePipeline")
} std::optional<uint32_t> GetPipelineSubgroupSize(PipelineHandle pipeline)
{
	EG_PANIC("Unimplemented")
}
void DestroyPipeline(PipelineHandle handle)
{
	EG_PANIC("Unimplemented: DestroyPipeline")
}
void BindPipeline(CommandContextHandle ctx, PipelineHandle handle)
{
	EG_PANIC("Unimplemented: BindPipeline")
}
void PushConstants(CommandContextHandle ctx, uint32_t offset, uint32_t range, const void* data)
{
	EG_PANIC("Unimplemented: PushConstants")
}

void DispatchCompute(CommandContextHandle cc, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
{
	EG_PANIC("Unimplemented: DispatchCompute")
}
void DispatchComputeIndirect(CommandContextHandle cc, BufferHandle argsBuffer, uint64_t argsBufferOffset)
{
	EG_PANIC("Unimplemented: DispatchComputeIndirect")
}

void SetViewport(CommandContextHandle ctx, float x, float y, float w, float h)
{
	EG_PANIC("Unimplemented: SetViewport")
}
void SetScissor(CommandContextHandle, int x, int y, int w, int h)
{
	EG_PANIC("Unimplemented: SetScissor")
}
void SetStencilValue(CommandContextHandle, StencilValue kind, uint32_t val)
{
	EG_PANIC("Unimplemented: SetStencilValue")
}
void SetWireframe(CommandContextHandle, bool wireframe)
{
	EG_PANIC("Unimplemented: SetWireframe")
}
void SetCullMode(CommandContextHandle, CullMode cullMode)
{
	EG_PANIC("Unimplemented: SetCullMode")
}
void BeginRenderPass(CommandContextHandle ctx, const RenderPassBeginInfo& beginInfo)
{
	EG_PANIC("Unimplemented: BeginRenderPass")
}
void EndRenderPass(CommandContextHandle ctx)
{
	EG_PANIC("Unimplemented: EndRenderPass")
}

void BindIndexBuffer(CommandContextHandle, IndexType type, BufferHandle buffer, uint32_t offset)
{
	EG_PANIC("Unimplemented: BindIndexBuffer")
}
void BindVertexBuffer(CommandContextHandle, uint32_t binding, BufferHandle buffer, uint32_t offset)
{
	EG_PANIC("Unimplemented: BindVertexBuffer")
}

void Draw(
	CommandContextHandle ctx, uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances)
{
	EG_PANIC("Unimplemented: Draw")
}
void DrawIndexed(
	CommandContextHandle, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance,
	uint32_t numInstances){ EG_PANIC("Unimplemented: DrawIndexed") }

QueryPoolHandle CreateQueryPool(QueryType type, uint32_t queryCount)
{
	EG_PANIC("Unimplemented: CreateQueryPool")
}
void DestroyQueryPool(QueryPoolHandle queryPool)
{
	EG_PANIC("Unimplemented: DestroyQueryPool")
}
bool GetQueryResults(QueryPoolHandle queryPool, uint32_t firstQuery, uint32_t numQueries, uint64_t dataSize, void* data)
{
	EG_PANIC("Unimplemented: GetQueryResults")
}
void CopyQueryResults(
	CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries,
	BufferHandle dstBufferHandle, uint64_t dstOffset)
{
	EG_PANIC("Unimplemented: CopyQueryResults")
}
void WriteTimestamp(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query)
{
	EG_PANIC("Unimplemented: WriteTimestamp")
}
void ResetQueries(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries)
{
	EG_PANIC("Unimplemented: ResetQueries")
}
void BeginQuery(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query)
{
	EG_PANIC("Unimplemented: BeginQuery")
}
void EndQuery(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query)
{
	EG_PANIC("Unimplemented: EndQuery")
}

void DebugLabelBegin(CommandContextHandle ctx, const char* label, const float* color)
{
	EG_PANIC("Unimplemented: DebugLabelBegin")
}
void DebugLabelEnd(CommandContextHandle ctx)
{
	EG_PANIC("Unimplemented: DebugLabelEnd")
}
void DebugLabelInsert(CommandContextHandle ctx, const char* label, const float* color)
{
	EG_PANIC("Unimplemented: DebugLabelInsert")
}
} // namespace eg::graphics_api::webgpu

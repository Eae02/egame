#include "../Abstraction.hpp"
#include "WGPU.hpp"
#include "WGPUBuffer.hpp"
#include "WGPUCommandContext.hpp"
#include "WGPUPipeline.hpp"

namespace eg::graphics_api::webgpu
{
void SetViewport(CommandContextHandle cc, float x, float y, float w, float h)
{
	CommandContext::Unwrap(cc).SetViewport({ .x = x, .y = y, .w = w, .h = h });
}

void SetScissor(CommandContextHandle cc, int x, int y, int w, int h)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);

	GraphicsPipeline& pipeline = std::get<GraphicsPipeline>(wcc.currentPipeline->pipeline);

	if (pipeline.enableScissorTest)
	{
		const int flippedY = std::max<int>(static_cast<int>(wcc.FramebufferHeight()) - (y + h), 0);
		wcc.SetScissor(ScissorRect{
			.x = static_cast<uint32_t>(std::max<int>(x, 0)),
			.y = static_cast<uint32_t>(flippedY),
			.w = static_cast<uint32_t>(glm::clamp(w, 0, ToInt(wcc.FramebufferWidth()) - x)),
			.h = static_cast<uint32_t>(glm::clamp(h, 0, ToInt(wcc.FramebufferHeight()) - flippedY)),
		});
	}
}

void SetStencilValue(CommandContextHandle cc, StencilValue kind, uint32_t val)
{
	EG_PANIC("Unimplemented: SetStencilValue")
}

void SetWireframe(CommandContextHandle cc, bool wireframe) {}

void SetCullMode(CommandContextHandle cc, CullMode cullMode)
{
	CommandContext::Unwrap(cc).SetDynamicCullMode(cullMode);
}

void BindIndexBuffer(CommandContextHandle cc, IndexType type, BufferHandle bufferHandle, uint32_t offset)
{
	WGPUIndexFormat indexFormat = type == IndexType::UInt32 ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16;

	Buffer& buffer = Buffer::Unwrap(bufferHandle);

	CommandContext& wcc = CommandContext::Unwrap(cc);
	wgpuRenderPassEncoderSetIndexBuffer(
		wcc.renderPassEncoder, buffer.buffer, indexFormat, offset, buffer.size - offset);
}

void BindVertexBuffer(CommandContextHandle cc, uint32_t binding, BufferHandle bufferHandle, uint32_t offset)
{
	Buffer& buffer = Buffer::Unwrap(bufferHandle);
	CommandContext& wcc = CommandContext::Unwrap(cc);

	wgpuRenderPassEncoderSetVertexBuffer(wcc.renderPassEncoder, binding, buffer.buffer, offset, buffer.size - offset);
}

void Draw(
	CommandContextHandle cc, uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);

	wcc.FlushDrawState();

	wgpuRenderPassEncoderDraw(wcc.renderPassEncoder, numVertices, numInstances, firstVertex, firstInstance);
}

void DrawIndexed(
	CommandContextHandle cc, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance,
	uint32_t numInstances)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);

	wcc.FlushDrawState();

	wgpuRenderPassEncoderDrawIndexed(
		wcc.renderPassEncoder, numIndices, numInstances, firstIndex, ToInt(firstVertex), firstInstance);
}

} // namespace eg::graphics_api::webgpu

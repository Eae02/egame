#include "WGPUCommandContext.hpp"
#include "WGPUBuffer.hpp"
#include "WGPUPipeline.hpp"

namespace eg::graphics_api::webgpu
{
CommandContext CommandContext::main;

void CommandContext::BeginEncode()
{
	if (commandBuffer != nullptr)
	{
		wgpuCommandBufferRelease(commandBuffer);
		commandBuffer = nullptr;
	}

	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoder = wgpuDeviceCreateCommandEncoder(wgpuctx.device, &encoderDesc);
}

void CommandContext::EndEncode()
{
	EndComputePass();

	WGPUCommandBufferDescriptor commandBufferDesc = {};
	commandBuffer = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);
	wgpuCommandEncoderRelease(encoder);
	encoder = nullptr;
}

static void BufferMappedCallback(WGPUBufferMapAsyncStatus status, void* userdata)
{
	Buffer& buffer = *static_cast<Buffer*>(userdata);

	if (status == WGPUBufferMapAsyncStatus_Success)
	{
		const void* mapPtr = wgpuBufferGetConstMappedRange(buffer.readbackBuffer, 0, buffer.size);
		std::memcpy(buffer.mapMemory.get(), mapPtr, buffer.size);
		wgpuBufferUnmap(buffer.readbackBuffer);
	}

	buffer.pendingReadback = false;
	buffer.Deref();
}

void CommandContext::Submit()
{
	wgpuQueueSubmit(wgpuctx.queue, 1, &commandBuffer);

	for (Buffer* buffer : m_readbackBuffers)
	{
		wgpuBufferMapAsync(buffer->readbackBuffer, WGPUMapMode_Read, 0, buffer->size, BufferMappedCallback, buffer);
	}
	m_readbackBuffers.clear();
}

void CommandContext::AddReadbackBuffer(Buffer& buffer)
{
	buffer.refCount++;
	m_readbackBuffers.push_back(&buffer);
}

void CommandContext::BeginRenderPass(
	const WGPURenderPassDescriptor& descriptor, uint32_t framebufferWidth, uint32_t framebufferHeight)
{
	EndComputePass();

	EG_ASSERT(renderPassEncoder == nullptr);

	renderPassEncoder = wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);

	m_framebufferWidth = framebufferWidth;
	m_framebufferHeight = framebufferHeight;

	m_renderState = {};
	m_renderState.viewport.w = static_cast<float>(framebufferWidth);
	m_renderState.viewport.h = static_cast<float>(framebufferHeight);
	m_renderState.scissorRect.w = framebufferWidth;
	m_renderState.scissorRect.h = framebufferHeight;
}

void CommandContext::SetViewport(const Viewport& viewport)
{
	if (m_renderState.viewport != viewport)
	{
		m_renderState.viewport = viewport;
		m_renderState.viewportChanged = true;
	}
}

void CommandContext::SetScissor(const std::optional<ScissorRect>& scissorRect)
{
	ScissorRect scissorRectUnwrapped = scissorRect.value_or(ScissorRect{
		.w = m_framebufferWidth,
		.h = m_framebufferHeight,
	});

	if (m_renderState.scissorRect != scissorRectUnwrapped)
	{
		m_renderState.scissorRect = scissorRectUnwrapped;
		m_renderState.scissorRectChanged = true;
	}
}

void CommandContext::SetDynamicCullMode(CullMode cullMode)
{
	if (m_renderState.dynamicCullMode != cullMode)
	{
		m_renderState.dynamicCullMode = cullMode;
		m_renderState.dynamiccullModeChanged = true;
	}
}

void CommandContext::EndComputePass()
{
	if (computePassEncoder != nullptr)
	{
		wgpuComputePassEncoderEnd(computePassEncoder);
		wgpuComputePassEncoderRelease(computePassEncoder);
		computePassEncoder = nullptr;
	}
}

void CommandContext::FlushDrawState()
{
	if (m_renderState.viewportChanged)
	{
		constexpr float MIN_DEPTH = 0.0f;
		constexpr float MAX_DEPTH = 1.0f;
		wgpuRenderPassEncoderSetViewport(
			renderPassEncoder, m_renderState.viewport.x, m_renderState.viewport.y, m_renderState.viewport.w,
			m_renderState.viewport.h, MIN_DEPTH, MAX_DEPTH);
		m_renderState.viewportChanged = false;
	}

	if (m_renderState.scissorRectChanged)
	{
		wgpuRenderPassEncoderSetScissorRect(
			renderPassEncoder, m_renderState.scissorRect.x, m_renderState.scissorRect.y, m_renderState.scissorRect.w,
			m_renderState.scissorRect.h);
		m_renderState.scissorRectChanged = false;
	}

	if (m_renderState.dynamiccullModeChanged)
	{
		GraphicsPipeline& graphicsPipeline = std::get<GraphicsPipeline>(currentPipeline->pipeline);
		if (graphicsPipeline.HasDynamicCullMode())
		{
			WGPURenderPipeline pipeline =
				graphicsPipeline.dynamicCullModePipelines.value()[static_cast<size_t>(m_renderState.dynamicCullMode)];

			wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipeline);

			m_renderState.dynamiccullModeChanged = false;
		}
	}
}

void DebugLabelBegin(CommandContextHandle cc, const char* label, const float* color) {}
void DebugLabelEnd(CommandContextHandle cc) {}
void DebugLabelInsert(CommandContextHandle cc, const char* label, const float* color) {}
} // namespace eg::graphics_api::webgpu

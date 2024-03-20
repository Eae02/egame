#include "WGPUCommandContext.hpp"
#include "WGPUBuffer.hpp"

namespace eg::graphics_api::webgpu
{
CommandContext CommandContext::main;

void CommandContext::BeginEncode()
{
	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoder = wgpuDeviceCreateCommandEncoder(wgpuctx.device, &encoderDesc);
}

void CommandContext::EndEncode()
{
	WGPUCommandBufferDescriptor commandBufferDesc = {};
	commandBuffer = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);
	wgpuCommandEncoderRelease(encoder);
	encoder = nullptr;
}

Fence* CommandContext::Submit()
{
	wgpuQueueSubmit(wgpuctx.queue, 1, &commandBuffer);

	return Fence::CreateAndInsert();
}

static inline WGPULoadOp TranslateLoadOp(AttachmentLoadOp loadOp)
{
	if (loadOp == AttachmentLoadOp::Load)
		return WGPULoadOp_Load;
	else
		return WGPULoadOp_Clear;
}

void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);

	WGPURenderPassColorAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];

	WGPURenderPassDescriptor descriptor = { .colorAttachments = colorAttachments };

	if (beginInfo.framebuffer == nullptr)
	{
		descriptor.colorAttachmentCount = 1;
		colorAttachments[0].view = wgpuctx.currentSwapchainColorTexture;
	}
	else
	{
		EG_PANIC("Unimplemented");
	}

	for (size_t i = 0; i < descriptor.colorAttachmentCount; i++)
	{
		colorAttachments[i].loadOp = TranslateLoadOp(beginInfo.colorAttachments[i].loadOp);
		colorAttachments[i].storeOp = WGPUStoreOp_Store;
	}

	wgpuCommandEncoderBeginRenderPass(wcc.encoder, &descriptor);
}

void EndRenderPass(CommandContextHandle cc)
{
	EG_PANIC("Unimplemented: EndRenderPass")
}

void DebugLabelBegin(CommandContextHandle cc, const char* label, const float* color) {}
void DebugLabelEnd(CommandContextHandle cc) {}
void DebugLabelInsert(CommandContextHandle cc, const char* label, const float* color) {}

static void FenceOnSubmittedWorkDoneCallback(WGPUQueueWorkDoneStatus status, void* userdata)
{
	Fence* fence = static_cast<Fence*>(userdata);
	fence->workDoneStatus = status;
	fence->semaphore.release();
	fence->Deref();
}

void Fence::Deref()
{
	if ((--refCount) <= 0)
		delete this;
}

bool Fence::IsDone() const
{
	wgpuDeviceTick(wgpuctx.device);
	return workDoneStatus.load() != WGPUQueueWorkDoneStatus_Force32;
}

void Fence::Wait()
{
	while (workDoneStatus.load() != WGPUQueueWorkDoneStatus_Force32)
		wgpuDeviceTick(wgpuctx.device);
}

Fence* Fence::CreateAndInsert()
{
	Fence* fence = new Fence;
	fence->refCount = 2;
	fence->workDoneStatus = WGPUQueueWorkDoneStatus_Force32;
	wgpuQueueOnSubmittedWorkDone(wgpuctx.queue, FenceOnSubmittedWorkDoneCallback, fence);

	return fence;
}
} // namespace eg::graphics_api::webgpu

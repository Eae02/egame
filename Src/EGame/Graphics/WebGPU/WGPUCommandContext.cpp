#include "WGPUCommandContext.hpp"

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
	return workDoneStatus.load() != WGPUQueueWorkDoneStatus_Force32;
}

void Fence::Wait()
{
	if (workDoneStatus.load() == WGPUQueueWorkDoneStatus_Force32)
		semaphore.acquire();
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

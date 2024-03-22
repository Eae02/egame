#include "WGPUFence.hpp"
#include "../../Assert.hpp"

namespace eg::graphics_api::webgpu
{
static void FenceOnSubmittedWorkDoneCallback(WGPUQueueWorkDoneStatus status, void* userdata)
{
	Fence* fence = static_cast<Fence*>(userdata);
	fence->workDoneStatus = status;
	fence->Deref();
}

void Fence::Deref()
{
	if ((--refCount) <= 0)
	{
		delete this;
	}
}

bool Fence::IsDone() const
{
	WGPUFutureWaitInfo waitInfo = { .future = future };
	WGPUWaitStatus status = wgpuInstanceWaitAny(wgpuctx.instance, 1, &waitInfo, 0);
	return status == WGPUWaitStatus_Success;

	// return workDoneStatus.load() != WGPUQueueWorkDoneStatus_Force32;
}

#ifndef __EMSCRIPTEN__
void Fence::Wait()
{
	wgpuDeviceTick(wgpuctx.device);

	WGPUFutureWaitInfo waitInfo = { .future = future };
	WGPUWaitStatus status = wgpuInstanceWaitAny(wgpuctx.instance, 1, &waitInfo, UINT64_MAX);
	EG_ASSERT(waitInfo.completed);
	EG_ASSERT(status == WGPUWaitStatus_Success);

	wgpuDeviceTick(wgpuctx.device);
}
#endif

Fence* Fence::CreateAndInsert()
{
	Fence* fence = new Fence;
	fence->refCount = 2;
	fence->workDoneStatus = WGPUQueueWorkDoneStatus_Force32;

#ifndef __EMSCRIPTEN__
	WGPUQueueWorkDoneCallbackInfo callbackInfo = {
		.mode = static_cast<WGPUCallbackMode>(WGPUCallbackMode_AllowSpontaneous | WGPUCallbackMode_AllowProcessEvents),
		.callback = FenceOnSubmittedWorkDoneCallback,
		.userdata = fence,
	};
	fence->future = wgpuQueueOnSubmittedWorkDoneF(wgpuctx.queue, callbackInfo);
#else
	wgpuQueueOnSubmittedWorkDone(wgpuctx.queue, FenceOnSubmittedWorkDoneCallback, fence);
#endif

	return fence;
}
} // namespace eg::graphics_api::webgpu

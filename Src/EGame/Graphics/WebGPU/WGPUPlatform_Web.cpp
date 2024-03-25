#ifdef __EMSCRIPTEN__

#include "WGPUPlatform.hpp"

namespace eg::graphics_api::webgpu
{
WGPUInstance PlatformInit(const GraphicsAPIInitArguments& initArguments)
{
	return wgpuCreateInstance(nullptr);
}

static void (*webRunFrameCallback)();

bool platformIsLoadingComplete = false;

int numFramesPending = 0;

void WorkDoneCallback(WGPUQueueWorkDoneStatus status, void*)
{
	platformIsLoadingComplete = true;

	numFramesPending--;

	while (numFramesPending < MAX_CONCURRENT_FRAMES)
	{
		webRunFrameCallback();
		numFramesPending++;
		wgpuQueueOnSubmittedWorkDone(wgpuctx.queue, WorkDoneCallback, nullptr);
	}
}

void StartWebMainLoop(void (*runFrame)())
{
	webRunFrameCallback = runFrame;
	numFramesPending++;
	wgpuQueueOnSubmittedWorkDone(wgpuctx.queue, WorkDoneCallback, nullptr);
}
} // namespace eg::graphics_api::webgpu

#endif

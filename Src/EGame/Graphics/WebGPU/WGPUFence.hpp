#pragma once

#include "WGPU.hpp"

#include <atomic>

namespace eg::graphics_api::webgpu
{
struct Fence
{
	WGPUFuture future;
	void Wait();

	std::atomic_int32_t refCount;
	std::atomic<WGPUQueueWorkDoneStatus> workDoneStatus;

	bool IsDone() const;

	void Deref();

	static Fence* CreateAndInsert();
};
} // namespace eg::graphics_api::webgpu

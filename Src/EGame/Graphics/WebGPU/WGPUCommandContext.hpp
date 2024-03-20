#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

#include <memory>
#include <semaphore>

namespace eg::graphics_api::webgpu
{
struct Fence
{
	std::binary_semaphore semaphore{ 0 };
	std::atomic_int32_t refCount;
	std::atomic<WGPUQueueWorkDoneStatus> workDoneStatus;

	bool IsDone() const;
	void Wait();

	void Deref();

	static Fence* CreateAndInsert();
};

class AbstractPipeline;

class CommandContext
{
public:
	CommandContext() = default;

	void BeginEncode();
	void EndEncode();
	Fence* Submit();

	static CommandContext main;

	static CommandContext& Unwrap(CommandContextHandle handle)
	{
		if (handle == nullptr)
			return main;
		return *reinterpret_cast<CommandContext*>(handle);
	}

	WGPUCommandBuffer commandBuffer = nullptr;

	WGPUCommandEncoder encoder = nullptr;
	WGPURenderPassEncoder renderPassEncoder = nullptr;
	WGPUComputePassEncoder computePassEncoder = nullptr;
	
	AbstractPipeline* currentPipeline = nullptr;

private:
};
} // namespace eg::graphics_api::webgpu

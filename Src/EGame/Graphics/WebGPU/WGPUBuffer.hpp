#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

#include <atomic>
#include <memory>

namespace eg::graphics_api::webgpu
{
struct Buffer
{
	WGPUBuffer buffer;
	uint64_t size;

	WGPUBuffer readbackBuffer = nullptr;

	std::unique_ptr<char[]> mapMemory;

	std::atomic_int32_t refCount{ 1 };
	std::atomic_bool pendingReadback{ false };

	void Deref();

	static Buffer& Unwrap(BufferHandle handle) { return *reinterpret_cast<Buffer*>(handle); }
};
} // namespace eg::graphics_api::webgpu

#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
struct Buffer
{
	WGPUBuffer buffer;
	uint64_t size;
	std::unique_ptr<char[]> allocationForPersistentMap;

	static Buffer& Unwrap(BufferHandle handle) { return *reinterpret_cast<Buffer*>(handle); }
};
} // namespace eg::graphics_api::webgpu

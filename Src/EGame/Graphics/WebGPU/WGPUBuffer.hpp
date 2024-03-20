#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
struct BufferMapData
{
	std::atomic_int refcount{ 1 };
	WGPUBuffer readbackBuffer{ nullptr };
	std::span<char> memory;

	void Deref();
	static BufferMapData* Alloc(uint64_t size);
};

struct Buffer
{
	WGPUBuffer buffer;
	uint64_t size;
	BufferMapData* mapData = nullptr;

	static Buffer& Unwrap(BufferHandle handle) { return *reinterpret_cast<Buffer*>(handle); }
};
} // namespace eg::graphics_api::webgpu

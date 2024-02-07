#pragma once

#include "MetalMain.hpp"

namespace eg::graphics_api::mtl
{
inline MTL::Buffer* UnwrapBuffer(BufferHandle buffer)
{
	return reinterpret_cast<MTL::Buffer*>(buffer);
}
} // namespace eg::graphics_api::mtl

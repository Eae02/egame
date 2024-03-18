#pragma once

#include "../../Hash.hpp"
#include "../Abstraction.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
struct Texture
{
	WGPUTexture texture;
	Format format;
	TextureViewType textureType;

	std::unordered_map<TextureViewKey, WGPUTextureView, MemberFunctionHash<TextureViewKey>> views;

	TextureSubresource ResolveSubresourceRem(TextureSubresource subresource) const;

	static Texture& Unwrap(TextureHandle handle) { return *reinterpret_cast<Texture*>(handle); }
};
} // namespace eg::graphics_api::webgpu

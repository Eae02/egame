#pragma once

#include "../../Hash.hpp"
#include "../Abstraction.hpp"
#include "WGPU.hpp"

#include <unordered_map>

namespace eg::graphics_api::webgpu
{
struct Texture
{
	WGPUTexture texture;
	Format format;
	TextureViewType textureType;

	std::unordered_map<TextureViewKey, WGPUTextureView, MemberFunctionHash<TextureViewKey>> views;

	TextureSubresource ResolveSubresourceRem(TextureSubresource subresource) const;

	WGPUTextureView GetTextureView(
		std::optional<TextureViewType> viewType, const TextureSubresource& subresource,
		Format viewFormat = Format::Undefined);

	static Texture& Unwrap(TextureHandle handle) { return *reinterpret_cast<Texture*>(handle); }
};

inline WGPUSampler UnwrapSampler(SamplerHandle sampler)
{
	return reinterpret_cast<WGPUSampler>(sampler);
}

inline WGPUTextureView UnwrapTextureView(TextureViewHandle textureView)
{
	return reinterpret_cast<WGPUTextureView>(textureView);
}
} // namespace eg::graphics_api::webgpu

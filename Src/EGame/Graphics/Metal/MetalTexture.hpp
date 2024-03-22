#pragma once

#include "../../Hash.hpp"
#include "MetalMain.hpp"

namespace eg::graphics_api::mtl
{
struct TextureViewKey
{
	MTL::TextureType type;
	MTL::PixelFormat format;
	TextureSubresource subresource;

	size_t Hash() const;
	bool operator==(const TextureViewKey& other) const;
};

struct Texture
{
	MTL::Texture* texture;
	Format format;

	std::unordered_map<TextureViewKey, MTL::Texture*, MemberFunctionHash<TextureViewKey>> views;

	MTL::Texture* GetTextureView(
		std::optional<TextureViewType> viewType, const TextureSubresource& subresource,
		Format format = Format::Undefined);

	static Texture& Unwrap(TextureHandle handle) { return *reinterpret_cast<Texture*>(handle); }
};

inline MTL::Texture* UnwrapTextureView(TextureViewHandle handle)
{
	return reinterpret_cast<MTL::Texture*>(handle);
}
} // namespace eg::graphics_api::mtl

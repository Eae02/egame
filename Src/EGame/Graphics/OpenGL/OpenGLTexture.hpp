#pragma once

#include "../../Hash.hpp"
#include "../AbstractionHL.hpp"
#include "../Format.hpp"
#include "GL.hpp"

#include <optional>
#include <unordered_map>

namespace eg::graphics_api::gl
{
struct Texture;

struct TextureViewKey
{
	GLenum type;
	Format format;
	TextureSubresource subresource;

	size_t Hash() const;
	bool operator==(const TextureViewKey& other) const;
};

struct TextureView
{
	TextureViewKey key;
	GLuint handle;
	GLenum glFormat;
	struct Texture* texture;

	void Bind(GLuint sampler, uint32_t glBinding) const;
	void BindAsStorageImage(uint32_t glBinding) const;
};

struct Texture
{
	GLuint texture;
	std::unordered_map<TextureViewKey, TextureView, MemberFunctionHash<TextureViewKey>> views;
	std::optional<SamplerDescription> samplerDescription;
	GLenum type;
	Format format;
	int dim;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t mipLevels;
	uint32_t arrayLayers;
	uint32_t sampleCount;
	TextureUsage currentUsage;
	std::string label;

	std::optional<GLuint> fbo;
	void LazyInitializeTextureFBO();

	void ChangeUsage(TextureUsage newUsage);
};

void BindTextureImpl(Texture& texture, GLuint handle, GLuint sampler, uint32_t glBinding);

inline Texture* UnwrapTexture(TextureHandle handle)
{
	return reinterpret_cast<Texture*>(handle);
}

inline TextureView* UnwrapTextureView(TextureViewHandle handle)
{
	return reinterpret_cast<TextureView*>(handle);
}
} // namespace eg::graphics_api::gl

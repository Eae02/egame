#include "OpenGL.hpp"
#include "OpenGLBuffer.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"
#include "Utils.hpp"

#include <GL/gl3w.h>

namespace eg::graphics_api::gl
{
	struct Texture
	{
		GLuint texture;
		Format format;
		int dim;
	};
	
	static ObjectPool<Texture> texturePool;
	
	Texture* UnwrapTexture(TextureHandle handle)
	{
		return reinterpret_cast<Texture*>(handle);
	}
	
	TextureHandle CreateTexture2D(const Texture2DCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		glCreateTextures(GL_TEXTURE_2D, 1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 2;
		
		GLenum format = TranslateFormat(createInfo.format);
		glTextureStorage2D(texture->texture, createInfo.mipLevels, format, createInfo.width, createInfo.height);
		
		glTextureParameteri(texture->texture, GL_TEXTURE_MAX_LEVEL, createInfo.mipLevels);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	static std::tuple<GLenum, GLenum> GetUploadFormat(Format format)
	{
		int componentCount = GetFormatComponentCount(format);
		int componentSize = GetFormatSize(format) / componentCount;
		
		const GLenum floatFormats[] = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };
		const GLenum integerFormats[] = { 0, GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_RGBA_INTEGER };
		
		const GLenum uTypes[] = { 0, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, 0, GL_UNSIGNED_INT };
		const GLenum sTypes[] = { 0, GL_BYTE, GL_SHORT, 0, GL_INT };
		
		switch (GetFormatType(format))
		{
		case FormatTypes::UNorm: return std::make_tuple(floatFormats[componentCount], uTypes[componentSize]);
		case FormatTypes::UInt: return std::make_tuple(integerFormats[componentCount], uTypes[componentSize]);
		case FormatTypes::SInt: return std::make_tuple(integerFormats[componentCount], sTypes[componentSize]);
		case FormatTypes::Float: return std::make_tuple(floatFormats[componentCount], GL_FLOAT);
		case FormatTypes::DepthStencil: EG_PANIC("Attempted to set the texture data for a depth/stencil texture.");
		}
		
		EG_UNREACHABLE
	}
	
	void SetTextureData(CommandContextHandle, TextureHandle handle, const TextureRange& range, const void* data)
	{
		Texture* texture = UnwrapTexture(handle);
		auto [format, type] = GetUploadFormat(texture->format);
		
		switch (texture->dim)
		{
		case 2:
			glTextureSubImage2D(texture->texture, range.mipLevel, range.offsetX, range.offsetY,
				range.sizeX, range.sizeY, format, type, data);
			break;
		}
	}
	
	void SetTextureDataBuffer(CommandContextHandle, TextureHandle handle, const TextureRange& range,
		BufferHandle buffer, uint64_t offset)
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, reinterpret_cast<const Buffer*>(buffer)->buffer);
		SetTextureData(nullptr, handle, range, reinterpret_cast<void*>(offset));
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
	
	void DestroyTexture(TextureHandle handle)
	{
		MainThreadInvoke([texture=UnwrapTexture(handle)]
		{
			glDeleteTextures(1, &texture->texture);
			texturePool.Free(texture);
		});
	}
	
	void BindTexture(CommandContextHandle, TextureHandle texture, uint32_t binding)
	{
		glBindTextureUnit(binding, UnwrapTexture(texture)->texture);
	}
}

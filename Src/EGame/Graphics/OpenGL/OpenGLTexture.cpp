#include "OpenGL.hpp"
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
	};
	
	static ObjectPool<Texture> texturePool;
	
	Texture* UnwrapTexture(TextureHandle handle)
	{
		return reinterpret_cast<Texture*>(handle);
	}
	
	TextureHandle CreateTexture2D(const Texture2DCreateInfo& createInfo, const void* initialData)
	{
		Texture* texture = texturePool.New();
		glCreateTextures(GL_TEXTURE_2D, 1, &texture->texture);
		
		GLenum format = TranslateFormat(createInfo.format);
		glTextureStorage2D(texture->texture, createInfo.mipLevels, format, createInfo.width, createInfo.height);
		
		glTextureParameteri(texture->texture, GL_TEXTURE_MAX_LEVEL, 0);
		glTextureParameteri(texture->texture, GL_TEXTURE_BASE_LEVEL, 0);
		
		if (initialData)
		{
			glTextureSubImage2D(texture->texture, 0, 0, 0, createInfo.width, createInfo.height, GL_RED, GL_UNSIGNED_BYTE, initialData);
		}
		
		return reinterpret_cast<TextureHandle>(texture);
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

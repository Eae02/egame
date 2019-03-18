#include "OpenGL.hpp"
#include "OpenGLTexture.hpp"
#include "OpenGLBuffer.hpp"
#include "OpenGLShader.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"
#include "Utils.hpp"

#include <GL/gl3w.h>

namespace eg::graphics_api::gl
{
	int maxAnistropy;
	
	static ObjectPool<Texture> texturePool;
	
	inline GLenum TranslateWrapMode(WrapMode wrapMode)
	{
		switch (wrapMode)
		{
		case WrapMode::Repeat:
			return GL_REPEAT;
		case WrapMode::MirroredRepeat:
			return GL_MIRRORED_REPEAT;
		case WrapMode::ClampToEdge:
			return GL_CLAMP_TO_EDGE;
		case WrapMode::ClampToBorder:
			return GL_CLAMP_TO_BORDER;
		}
		
		EG_UNREACHABLE
	}
	
	inline GLenum GetMinFilter(const SamplerDescription& description)
	{
		if (description.mipFilter == TextureFilter::Linear)
		{
			if (description.minFilter == TextureFilter::Linear)
				return GL_LINEAR_MIPMAP_LINEAR;
			else
				return GL_NEAREST_MIPMAP_LINEAR;
		}
		else
		{
			if (description.minFilter == TextureFilter::Linear)
				return GL_LINEAR_MIPMAP_NEAREST;
			else
				return GL_NEAREST_MIPMAP_NEAREST;
		}
	}
	
	inline GLenum GetMagFilter(TextureFilter magFilter)
	{
		if (magFilter == TextureFilter::Linear)
			return GL_LINEAR;
		else
			return GL_NEAREST;
	}
	
	inline std::array<float, 4> TranslateBorderColor(BorderColor color)
	{
		switch (color)
		{
		case BorderColor::F0000:
		case BorderColor::I0000:
			return {0.0f, 0.0f, 0.0f, 0.0f};
		case BorderColor::F0001:
		case BorderColor::I0001:
			return {0.0f, 0.0f, 0.0f, 1.0f};
		case BorderColor::F1111:
		case BorderColor::I1111:
			return {1.0f, 1.0f, 1.0f, 1.0f};
		}
		
		EG_UNREACHABLE
	}
	
	inline float ClampMaxAnistropy(int _maxAnistropy)
	{
		return glm::clamp(_maxAnistropy, 1, maxAnistropy);
	}
	
	SamplerHandle CreateSampler(const SamplerDescription& description)
	{
		auto borderColor = TranslateBorderColor(description.borderColor);
		
		GLuint sampler;
		glCreateSamplers(1, &sampler);
		
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GetMinFilter(description));
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GetMagFilter(description.magFilter));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, TranslateWrapMode(description.wrapU));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, TranslateWrapMode(description.wrapV));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, TranslateWrapMode(description.wrapW));
		glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, ClampMaxAnistropy(description.maxAnistropy));
		glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, description.mipLodBias);
		glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, borderColor.data());
		
		return reinterpret_cast<SamplerHandle>(sampler);
	}
	
	void DestroySampler(SamplerHandle handle)
	{
		MainThreadInvoke([sampler = static_cast<GLuint>(reinterpret_cast<uintptr_t>(handle))]
		{
			glDeleteSamplers(1, &sampler);
		});
	}
	
	static GLenum TranslateSwizzle(SwizzleMode mode, GLenum identity)
	{
		switch (mode)
		{
		case SwizzleMode::Identity:
			return identity;
		case SwizzleMode::One:
			return GL_ONE;
		case SwizzleMode::Zero:
			return GL_ZERO;
		case SwizzleMode::R:
			return GL_RED;
		case SwizzleMode::G:
			return GL_GREEN;
		case SwizzleMode::B:
			return GL_BLUE;
		case SwizzleMode::A:
			return GL_ALPHA;
		}
		
		EG_UNREACHABLE
	}
	
	static void InitTexture(GLuint texture, const TextureCreateInfo& createInfo)
	{
		glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, createInfo.mipLevels);
		
		if (createInfo.defaultSamplerDescription != nullptr)
		{
			const SamplerDescription& samplerDesc = *createInfo.defaultSamplerDescription;
			
			auto borderColor = TranslateBorderColor(samplerDesc.borderColor);
			
			glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GetMinFilter(samplerDesc));
			glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GetMagFilter(samplerDesc.magFilter));
			glTextureParameteri(texture, GL_TEXTURE_WRAP_S, TranslateWrapMode(samplerDesc.wrapU));
			glTextureParameteri(texture, GL_TEXTURE_WRAP_T, TranslateWrapMode(samplerDesc.wrapV));
			glTextureParameteri(texture, GL_TEXTURE_WRAP_R, TranslateWrapMode(samplerDesc.wrapW));
			glTextureParameterf(texture, GL_TEXTURE_MAX_ANISOTROPY, ClampMaxAnistropy(samplerDesc.maxAnistropy));
			glTextureParameterf(texture, GL_TEXTURE_LOD_BIAS, samplerDesc.mipLodBias);
			glTextureParameterfv(texture, GL_TEXTURE_BORDER_COLOR, borderColor.data());
		}
		
		glTextureParameteri(texture, GL_TEXTURE_SWIZZLE_R, TranslateSwizzle(createInfo.swizzleR, GL_RED));
		glTextureParameteri(texture, GL_TEXTURE_SWIZZLE_G, TranslateSwizzle(createInfo.swizzleG, GL_GREEN));
		glTextureParameteri(texture, GL_TEXTURE_SWIZZLE_B, TranslateSwizzle(createInfo.swizzleB, GL_BLUE));
		glTextureParameteri(texture, GL_TEXTURE_SWIZZLE_A, TranslateSwizzle(createInfo.swizzleA, GL_ALPHA));
	}
	
	TextureHandle CreateTexture2D(const Texture2DCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		glCreateTextures(GL_TEXTURE_2D, 1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 2;
		texture->width = createInfo.width;
		texture->height = createInfo.height;
		
		GLenum format = TranslateFormat(createInfo.format);
		glTextureStorage2D(texture->texture, createInfo.mipLevels, format, createInfo.width, createInfo.height);
		
		InitTexture(texture->texture, createInfo);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	TextureHandle CreateTexture2DArray(const Texture2DArrayCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 3;
		texture->width = createInfo.width;
		texture->height = createInfo.height;
		
		GLenum format = TranslateFormat(createInfo.format);
		glTextureStorage3D(texture->texture, createInfo.mipLevels, format,
		                   createInfo.width, createInfo.height, createInfo.arrayLayers);
		
		InitTexture(texture->texture, createInfo);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	TextureHandle CreateTextureCube(const TextureCubeCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 3;
		texture->width = createInfo.width;
		texture->height = createInfo.width;
		
		GLenum format = TranslateFormat(createInfo.format);
		glTextureStorage2D(texture->texture, createInfo.mipLevels, format,
		                   createInfo.width, createInfo.width);
		
		InitTexture(texture->texture, createInfo);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	TextureHandle CreateTextureCubeArray(const TextureCubeArrayCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 3;
		texture->width = createInfo.width;
		texture->height = createInfo.width;
		
		GLenum format = TranslateFormat(createInfo.format);
		glTextureStorage3D(texture->texture, createInfo.mipLevels, format,
		                   createInfo.width, createInfo.width, 6 * createInfo.arrayLayers);
		
		InitTexture(texture->texture, createInfo);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	static std::tuple<GLenum, GLenum> GetUploadFormat(Format format)
	{
		int componentCount = GetFormatComponentCount(format);
		int componentSize = GetFormatSize(format) / componentCount;
		
		const GLenum floatFormats[] = {0, GL_RED, GL_RG, GL_RGB, GL_RGBA};
		const GLenum integerFormats[] = {0, GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_RGBA_INTEGER};
		
		const GLenum uTypes[] = {0, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, 0, GL_UNSIGNED_INT};
		const GLenum sTypes[] = {0, GL_BYTE, GL_SHORT, 0, GL_INT};
		
		switch (GetFormatType(format))
		{
		case FormatTypes::UNorm:
			return std::make_tuple(floatFormats[componentCount], uTypes[componentSize]);
		case FormatTypes::UInt:
			return std::make_tuple(integerFormats[componentCount], uTypes[componentSize]);
		case FormatTypes::SInt:
			return std::make_tuple(integerFormats[componentCount], sTypes[componentSize]);
		case FormatTypes::Float:
			return std::make_tuple(floatFormats[componentCount], GL_FLOAT);
		case FormatTypes::DepthStencil:
		EG_PANIC("Attempted to set the texture data for a depth/stencil texture.");
		}
		
		EG_UNREACHABLE
	}
	
	void SetTextureData(CommandContextHandle, TextureHandle handle, const TextureRange& range,
		BufferHandle buffer, uint64_t offset)
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, reinterpret_cast<const Buffer*>(buffer)->buffer);
		
		Texture* texture = UnwrapTexture(handle);
		auto[format, type] = GetUploadFormat(texture->format);
		
		switch (texture->dim)
		{
		case 2:
			glTextureSubImage2D(texture->texture, range.mipLevel, range.offsetX, range.offsetY,
			                    range.sizeX, range.sizeY, format, type, (void*)(uintptr_t)offset);
			break;
		case 3:
			glTextureSubImage3D(texture->texture, range.mipLevel, range.offsetX, range.offsetY, range.offsetZ,
			                    range.sizeX, range.sizeY, range.sizeZ, format, type, (void*)(uintptr_t)offset);
			break;
		}
		
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
	
	void GenerateMipmaps(CommandContextHandle, TextureHandle handle)
	{
		glGenerateTextureMipmap(UnwrapTexture(handle)->texture);
	}
	
	void DestroyTexture(TextureHandle handle)
	{
		MainThreadInvoke([texture = UnwrapTexture(handle)]
		{
			glDeleteTextures(1, &texture->texture);
			texturePool.Free(texture);
		});
	}
	
	void BindTexture(CommandContextHandle, TextureHandle texture, SamplerHandle sampler,
		uint32_t set, uint32_t binding)
	{
		uint32_t glBinding = ResolveBinding(set, binding);
		glBindSampler(glBinding, (GLuint)reinterpret_cast<uintptr_t>(sampler));
		glBindTextureUnit(glBinding, UnwrapTexture(texture)->texture);
	}
	
	void ClearColorTexture(CommandContextHandle, TextureHandle handle, uint32_t mipLevel, const Color& color)
	{
		const Texture* texture = UnwrapTexture(handle);
		glClearTexImage(texture->texture, mipLevel, GL_RGBA, GL_FLOAT, &color.r);
	}
	
	void TextureUsageHint(TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags) { }
}

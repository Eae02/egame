#include "OpenGL.hpp"
#include "OpenGLTexture.hpp"
#include "OpenGLBuffer.hpp"
#include "Pipeline.hpp"
#include "Utils.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"

#ifndef __EMSCRIPTEN__
#include <GL/glext.h>
#endif

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
		glGenSamplers(1, &sampler);
		
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GetMinFilter(description));
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GetMagFilter(description.magFilter));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, TranslateWrapMode(description.wrapU));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, TranslateWrapMode(description.wrapV));
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, TranslateWrapMode(description.wrapW));
		glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, ClampMaxAnistropy(description.maxAnistropy));
#ifndef EG_GLES
		glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, description.mipLodBias);
#endif
		glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, borderColor.data());
		
		if (description.enableCompare)
		{
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, TranslateCompareOp(description.compareOp));
		}
		
		return reinterpret_cast<SamplerHandle>(sampler);
	}
	
	void DestroySampler(SamplerHandle handle)
	{
		MainThreadInvoke([sampler = static_cast<GLuint>(reinterpret_cast<uintptr_t>(handle))]
		{
			glDeleteSamplers(1, &sampler);
		});
	}
	
	static void InitTexture(const Texture& texture, const TextureCreateInfo& createInfo)
	{
#ifndef __EMSCRIPTEN__
		if (createInfo.label != nullptr)
		{
			glObjectLabel(GL_TEXTURE, texture.texture, -1, createInfo.label);
		}
#endif
		
		glTexParameteri(texture.type, GL_TEXTURE_MAX_LEVEL, createInfo.mipLevels);
		
		if (createInfo.defaultSamplerDescription != nullptr && createInfo.sampleCount == 1)
		{
			const SamplerDescription& samplerDesc = *createInfo.defaultSamplerDescription;
			
			auto borderColor = TranslateBorderColor(samplerDesc.borderColor);
			
			glTexParameteri(texture.type, GL_TEXTURE_MIN_FILTER, GetMinFilter(samplerDesc));
			glTexParameteri(texture.type, GL_TEXTURE_MAG_FILTER, GetMagFilter(samplerDesc.magFilter));
			glTexParameteri(texture.type, GL_TEXTURE_WRAP_S, TranslateWrapMode(samplerDesc.wrapU));
			glTexParameteri(texture.type, GL_TEXTURE_WRAP_T, TranslateWrapMode(samplerDesc.wrapV));
			glTexParameteri(texture.type, GL_TEXTURE_WRAP_R, TranslateWrapMode(samplerDesc.wrapW));
			glTexParameterf(texture.type, GL_TEXTURE_MAX_ANISOTROPY_EXT, ClampMaxAnistropy(samplerDesc.maxAnistropy));
			
#ifndef __EMSCRIPTEN__
			glTexParameterfv(texture.type, GL_TEXTURE_BORDER_COLOR, borderColor.data());
#endif
			
#ifndef EG_GLES
			glTexParameterf(texture.type, GL_TEXTURE_LOD_BIAS, samplerDesc.mipLodBias);
#endif
			
			if (samplerDesc.enableCompare)
			{
				glTexParameteri(texture.type, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
				glTexParameteri(texture.type, GL_TEXTURE_COMPARE_FUNC, TranslateCompareOp(samplerDesc.compareOp));
			}
		}
	}
	
	TextureHandle CreateTexture2D(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		texture->type = createInfo.sampleCount == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
		glGenTextures(1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 2;
		texture->width = createInfo.width;
		texture->height = createInfo.height;
		texture->depth = 1;
		texture->mipLevels = createInfo.mipLevels;
		texture->sampleCount = createInfo.sampleCount;
		texture->arrayLayers = 1;
		texture->currentUsage = TextureUsage::Undefined;
		
		glBindTexture(texture->type, texture->texture);
		
		GLenum format = TranslateFormat(createInfo.format);
		if (createInfo.sampleCount == 1)
		{
			glTexStorage2D(texture->type, createInfo.mipLevels, format, createInfo.width, createInfo.height);
		}
		else
		{
#ifdef __EMSCRIPTEN__
			EG_PANIC("Multisampling is not supported in WebGL")
#else
			glTexStorage2DMultisample(texture->type, createInfo.sampleCount, format,
				createInfo.width, createInfo.height, GL_FALSE);
#endif
		}
		
		InitTexture(*texture, createInfo);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	TextureHandle CreateTexture2DArray(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		texture->type = createInfo.sampleCount == 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
		glGenTextures(1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 3;
		texture->width = createInfo.width;
		texture->height = createInfo.height;
		texture->depth = 1;
		texture->mipLevels = createInfo.mipLevels;
		texture->sampleCount = createInfo.sampleCount;
		texture->arrayLayers = createInfo.arrayLayers;
		texture->currentUsage = TextureUsage::Undefined;
		
		glBindTexture(texture->type, texture->texture);
		
		GLenum format = TranslateFormat(createInfo.format);
		if (createInfo.sampleCount == 1)
		{
			glTexStorage3D(texture->type, createInfo.mipLevels, format,
				createInfo.width, createInfo.height, createInfo.arrayLayers);
		}
		else
		{
#ifdef __EMSCRIPTEN__
			EG_PANIC("Multisampling is not supported in WebGL")
#else
			glTexStorage3DMultisample(texture->type, createInfo.sampleCount, format,
				createInfo.width, createInfo.height, createInfo.arrayLayers, GL_FALSE);
#endif
		}
		
		InitTexture(*texture, createInfo);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	TextureHandle CreateTextureCube(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		texture->type = GL_TEXTURE_CUBE_MAP;
		glGenTextures(1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 3;
		texture->width = createInfo.width;
		texture->height = createInfo.width;
		texture->depth = 1;
		texture->mipLevels = createInfo.mipLevels;
		texture->sampleCount = 1;
		texture->arrayLayers = 6;
		texture->currentUsage = TextureUsage::Undefined;
		
		glBindTexture(texture->type, texture->texture);
		
		GLenum format = TranslateFormat(createInfo.format);
		glTexStorage2D(texture->type, createInfo.mipLevels, format, createInfo.width, createInfo.width);
		
		InitTexture(*texture, createInfo);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	TextureHandle CreateTextureCubeArray(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		texture->type = GL_TEXTURE_CUBE_MAP_ARRAY;
		glGenTextures(1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 3;
		texture->width = createInfo.width;
		texture->height = createInfo.width;
		texture->depth = 1;
		texture->mipLevels = createInfo.mipLevels;
		texture->sampleCount = 1;
		texture->arrayLayers = 6 * createInfo.arrayLayers;
		texture->currentUsage = TextureUsage::Undefined;
		
		glBindTexture(texture->type, texture->texture);
		
		GLenum format = TranslateFormat(createInfo.format);
		glTexStorage3D(texture->texture, createInfo.mipLevels, format,
		                   createInfo.width, createInfo.width, texture->arrayLayers);
		
		InitTexture(*texture, createInfo);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	TextureHandle CreateTexture3D(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		texture->type = GL_TEXTURE_3D;
		glGenTextures(1, &texture->texture);
		
		texture->format = createInfo.format;
		texture->dim = 3;
		texture->width = createInfo.width;
		texture->height = createInfo.height;
		texture->depth = createInfo.depth;
		texture->mipLevels = 1;
		texture->sampleCount = 1;
		texture->arrayLayers = 1;
		texture->currentUsage = TextureUsage::Undefined;
		
		glBindTexture(texture->type, texture->texture);
		
		GLenum format = TranslateFormat(createInfo.format);
		glTexStorage3D(texture->type, createInfo.mipLevels, format, createInfo.width, createInfo.height, createInfo.depth);
		
		InitTexture(*texture, createInfo);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	GLuint Texture::GetView(const TextureSubresource& subresource)
	{
		TextureSubresource resolvedSubresource = subresource.ResolveRem(mipLevels, arrayLayers);
		if (resolvedSubresource.firstMipLevel == 0 && resolvedSubresource.numMipLevels == mipLevels &&
			resolvedSubresource.firstArrayLayer == 0 && resolvedSubresource.numArrayLayers == arrayLayers)
		{
			return texture;
		}
		
#ifdef EG_GLES
		eg::Log(LogLevel::Error, "gl", "Texture views not supported in GLES");
		return texture;
#else
		for (const TextureView& view : views)
		{
			if (view.subresource == resolvedSubresource)
			{
				return view.texture;
			}
		}
		
		TextureView& view = views.emplace_back();
		
		GLenum viewType = type;
		if (viewType == GL_TEXTURE_2D_ARRAY && resolvedSubresource.numArrayLayers == 1)
			viewType = GL_TEXTURE_2D;
		if (viewType == GL_TEXTURE_CUBE_MAP_ARRAY && resolvedSubresource.numArrayLayers == 6)
			viewType = GL_TEXTURE_CUBE_MAP;
		
		glGenTextures(1, &view.texture);
		glTextureView(view.texture, viewType, texture, TranslateFormat(format),
			resolvedSubresource.firstMipLevel, resolvedSubresource.numMipLevels,
			resolvedSubresource.firstArrayLayer, resolvedSubresource.numArrayLayers);
		
		view.subresource = resolvedSubresource;
		
		return view.texture;
#endif
	}
	
	static std::pair<Format, GLenum> compressedUploadFormats[] =
	{
		{ Format::BC1_RGBA_UNorm, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT },
		{ Format::BC1_RGBA_sRGB, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT },
		{ Format::BC1_RGB_UNorm, GL_COMPRESSED_RGB_S3TC_DXT1_EXT },
		{ Format::BC1_RGB_sRGB, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT },
		{ Format::BC3_UNorm, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT },
		{ Format::BC3_sRGB, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT },
		{ Format::BC4_UNorm, GL_COMPRESSED_RED_RGTC1 },
		{ Format::BC5_UNorm, GL_COMPRESSED_RG_RGTC2 }
	};
	
	static std::tuple<GLenum, GLenum> GetUploadFormat(Format format)
	{
		for (const std::pair<Format, GLenum>& compressedFormat : compressedUploadFormats)
		{
			if (compressedFormat.first == format)
				return std::make_tuple(compressedFormat.second, 0);
		}
		
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
		BufferHandle bufferHandle, uint64_t offset)
	{
		void* offsetPtr = nullptr;
		const Buffer* buffer = UnwrapBuffer(bufferHandle);
		
#ifdef EG_GLES
		if (buffer->isHostBuffer)
		{
			offsetPtr = buffer->persistentMapping + offset;
		}
#endif
		
		if (offsetPtr == nullptr)
		{
			offsetPtr = (void*)(uintptr_t)offset;
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer->buffer);
		}
		
		Texture* texture = UnwrapTexture(handle);
		auto[format, type] = GetUploadFormat(texture->format);
		
		texture->ChangeUsage(TextureUsage::CopyDst);
		
		const bool isCompressed = IsCompressedFormat(texture->format);
		const uint32_t imageBytes = GetImageByteSize(range.sizeX, range.sizeY, texture->format);
		
		glBindTexture(texture->type, texture->texture);
		
		switch (texture->dim)
		{
		case 2:
			if (isCompressed)
			{
				glCompressedTexSubImage2D(texture->type, range.mipLevel, range.offsetX, range.offsetY,
					range.sizeX, range.sizeY, format, imageBytes, offsetPtr);
			}
			else
			{
				glTexSubImage2D(texture->type, range.mipLevel, range.offsetX, range.offsetY,
				                range.sizeX, range.sizeY, format, type, offsetPtr);
			}
			break;
		case 3:
			if (isCompressed)
			{
				glCompressedTexSubImage3D(texture->type, range.mipLevel, range.offsetX, range.offsetY,
					range.offsetZ, range.sizeX, range.sizeY, range.sizeZ, format, imageBytes * range.sizeZ, offsetPtr);
			}
			else
			{
				glTexSubImage3D(texture->type, range.mipLevel, range.offsetX, range.offsetY, range.offsetZ,
				                range.sizeX, range.sizeY, range.sizeZ, format, type, offsetPtr);
			}
			break;
		}
		
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
	
	void GenerateMipmaps(CommandContextHandle, TextureHandle handle)
	{
		Texture* texture = UnwrapTexture(handle);
		glBindTexture(texture->type, texture->texture);
		glGenerateMipmap(texture->texture);
	}
	
	void DestroyTexture(TextureHandle handle)
	{
		MainThreadInvoke([texture = UnwrapTexture(handle)]
		{
			glDeleteTextures(1, &texture->texture);
			if (texture->hasBlitFBO)
				glDeleteFramebuffers(1, &texture->blitFBO);
			texturePool.Free(texture);
		});
	}
	
	void BindTexture(CommandContextHandle, TextureHandle texture, SamplerHandle sampler, uint32_t set, uint32_t binding,
		const TextureSubresource& subresource)
	{
		uint32_t glBinding = ResolveBinding(set, binding);
		glBindSampler(glBinding, (GLuint)reinterpret_cast<uintptr_t>(sampler));
		glActiveTexture(GL_TEXTURE0 + glBinding);
		Texture* tex = UnwrapTexture(texture);
		glBindTexture(tex->type, tex->GetView(subresource));
	}
	
	void Texture::BindAsStorageImage(uint32_t glBinding, const TextureSubresource& subresource)
	{
#ifndef __EMSCRIPTEN__
		glBindImageTexture(glBinding, GetView(subresource), 0, GL_TRUE, 0, GL_READ_WRITE, TranslateFormat(format));
#endif
	}
	
	void Texture::MaybeInitBlitFBO()
	{
		if (hasBlitFBO)
			return;
		
		GLenum target = GetFormatType(format) == FormatTypes::DepthStencil ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
		
		hasBlitFBO = true;
		glGenFramebuffers(1, &blitFBO);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, blitFBO);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, target, GL_TEXTURE_2D, texture, 0);
	}
	
	void BindStorageImage(CommandContextHandle, TextureHandle textureHandle, uint32_t set, uint32_t binding,
		const TextureSubresourceLayers& subresource)
	{
		UnwrapTexture(textureHandle)->BindAsStorageImage(ResolveBinding(set, binding), subresource.AsSubresource());
	}
	
	void ClearColorTexture(CommandContextHandle, TextureHandle handle, uint32_t mipLevel, const void* color)
	{
#ifdef EG_GLES
		Log(LogLevel::Error, "gl", "ClearColorTexture not available in GLES");
#else
		const Texture* texture = UnwrapTexture(handle);
		auto [format, type] = GetUploadFormat(texture->format);
		glClearTexImage(texture->texture, mipLevel, format, type, color);
#endif
	}
	
	void ResolveTexture(CommandContextHandle, TextureHandle srcHandle, TextureHandle dstHandle, const ResolveRegion& region)
	{
		Texture* src = UnwrapTexture(srcHandle);
		Texture* dst = UnwrapTexture(dstHandle);
		
		src->MaybeInitBlitFBO();
		dst->MaybeInitBlitFBO();
		
		GLenum blitBuffer = GL_COLOR_BUFFER_BIT;
		if (GetFormatType(src->format) == FormatTypes::DepthStencil)
			blitBuffer = GL_DEPTH_BUFFER_BIT;
		
		glBindFramebuffer(GL_READ_FRAMEBUFFER, src->blitFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->blitFBO);
		
		glBlitFramebuffer(region.srcOffset.x, region.srcOffset.y, region.srcOffset.x + region.width, region.srcOffset.y + region.height,
			region.dstOffset.x, region.dstOffset.y, region.dstOffset.x + region.width, region.dstOffset.y + region.height,
			blitBuffer, GL_NEAREST);
	}
	
	inline void MaybeBarrierAfterILS(TextureUsage newUsage)
	{
		switch (newUsage)
		{
		case TextureUsage::Undefined:break;
		case TextureUsage::CopySrc:
		case TextureUsage::CopyDst:
			MaybeInsertBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
			break;
		case TextureUsage::ShaderSample:
			MaybeInsertBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
			break;
		case TextureUsage::FramebufferAttachment:
			MaybeInsertBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
			break;
		case TextureUsage::ILSRead:
		case TextureUsage::ILSWrite:
		case TextureUsage::ILSReadWrite:
			MaybeInsertBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			break;
		}
	}
	
	void TextureBarrier(CommandContextHandle cc, TextureHandle handle, const eg::TextureBarrier& barrier)
	{
		if (barrier.oldUsage == TextureUsage::ILSWrite || barrier.oldUsage == TextureUsage::ILSReadWrite)
		{
			MaybeBarrierAfterILS(barrier.newUsage);
		}
	}
	
	void TextureUsageHint(TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags)
	{
		UnwrapTexture(handle)->ChangeUsage(newUsage);
	}
	
	void Texture::ChangeUsage(TextureUsage newUsage)
	{
		if (currentUsage == TextureUsage::ILSWrite || currentUsage == TextureUsage::ILSReadWrite)
		{
			MaybeBarrierAfterILS(newUsage);
		}
		
		currentUsage = newUsage;
	}
}

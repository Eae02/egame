#include "OpenGLTexture.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "../../Hash.hpp"
#include "../../MainThreadInvoke.hpp"
#include "Framebuffer.hpp"
#include "OpenGL.hpp"
#include "OpenGLBuffer.hpp"
#include "Pipeline.hpp"
#include "PipelineGraphics.hpp"
#include "Utils.hpp"

namespace eg::graphics_api::gl
{
int maxAnistropy;

static ObjectPool<Texture> texturePool;

inline GLenum TranslateWrapMode(WrapMode wrapMode)
{
	switch (wrapMode)
	{
	case WrapMode::Repeat: return GL_REPEAT;
	case WrapMode::MirroredRepeat: return GL_MIRRORED_REPEAT;
	case WrapMode::ClampToEdge: return GL_CLAMP_TO_EDGE;
	case WrapMode::ClampToBorder:
#ifdef __EMSCRIPTEN__
		EG_PANIC("WrapMode::ClampToBorder is not supported in WebGL");
#endif
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
	case BorderColor::I0000: return { 0.0f, 0.0f, 0.0f, 0.0f };
	case BorderColor::F0001:
	case BorderColor::I0001: return { 0.0f, 0.0f, 0.0f, 1.0f };
	case BorderColor::F1111:
	case BorderColor::I1111: return { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	EG_UNREACHABLE
}

inline int ClampMaxAnistropy(int _maxAnistropy)
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
#ifndef __EMSCRIPTEN__
	glSamplerParameterf(
		sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, static_cast<float>(ClampMaxAnistropy(description.maxAnistropy)));
	glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, description.mipLodBias);
	glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, borderColor.data());
#endif

	if (description.enableCompare)
	{
		glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, TranslateCompareOp(description.compareOp));
	}

	return reinterpret_cast<SamplerHandle>(static_cast<uintptr_t>(sampler));
}

static void InitTexture(Texture& texture, const TextureCreateInfo& createInfo)
{
	if (createInfo.label != nullptr)
	{
		glObjectLabel(GL_TEXTURE, texture.texture, -1, createInfo.label);
		texture.label = createInfo.label;
	}

	glTexParameteri(texture.type, GL_TEXTURE_MAX_LEVEL, createInfo.mipLevels);
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

	GLenum format = TranslateFormatForTexture(createInfo.format);
	if (createInfo.sampleCount == 1)
	{
		glTexStorage2D(texture->type, createInfo.mipLevels, format, createInfo.width, createInfo.height);
	}
	else
	{
#ifdef __EMSCRIPTEN__
		EG_PANIC("Multisampling is not supported in WebGL")
#else
		glTexStorage2DMultisample(
			texture->type, createInfo.sampleCount, format, createInfo.width, createInfo.height, GL_FALSE);
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

	GLenum format = TranslateFormatForTexture(createInfo.format);
	if (createInfo.sampleCount == 1)
	{
		glTexStorage3D(
			texture->type, createInfo.mipLevels, format, createInfo.width, createInfo.height, createInfo.arrayLayers);
	}
	else
	{
#ifdef __EMSCRIPTEN__
		EG_PANIC("Multisampling is not supported in WebGL")
#else
		glTexStorage3DMultisample(
			texture->type, createInfo.sampleCount, format, createInfo.width, createInfo.height, createInfo.arrayLayers,
			GL_FALSE);
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

	GLenum format = TranslateFormatForTexture(createInfo.format);
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

	GLenum format = TranslateFormatForTexture(createInfo.format);
	glTexStorage3D(
		texture->texture, createInfo.mipLevels, format, createInfo.width, createInfo.width, texture->arrayLayers);

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

	GLenum format = TranslateFormatForTexture(createInfo.format);
	glTexStorage3D(texture->type, createInfo.mipLevels, format, createInfo.width, createInfo.height, createInfo.depth);

	InitTexture(*texture, createInfo);

	return reinterpret_cast<TextureHandle>(texture);
}

static inline GLenum TranslateViewType(const Texture& texture, TextureViewType viewType)
{
	switch (viewType)
	{
	case TextureViewType::SameAsTexture: return texture.type;
	case TextureViewType::Flat2D: return GL_TEXTURE_2D;
	case TextureViewType::Flat3D: return GL_TEXTURE_3D;
	case TextureViewType::Cube: return GL_TEXTURE_CUBE_MAP;
	case TextureViewType::Array2D: return GL_TEXTURE_2D_ARRAY;
	case TextureViewType::ArrayCube: return GL_TEXTURE_CUBE_MAP_ARRAY;
	default: EG_UNREACHABLE
	}
}

TextureViewHandle GetTextureView(
	TextureHandle textureHandle, TextureViewType viewType, const TextureSubresource& subresource, Format format)
{
	Texture* texture = UnwrapTexture(textureHandle);

	TextureViewKey viewKey;
	viewKey.type = TranslateViewType(*texture, viewType);
	viewKey.subresource = subresource.ResolveRem(texture->mipLevels, texture->arrayLayers);
	viewKey.format = format == Format::Undefined ? texture->format : format;

	auto it = texture->views.find(viewKey);
	if (it != texture->views.end())
		return reinterpret_cast<TextureViewHandle>(&it->second);

	GLenum glFormat = TranslateFormatForTexture(viewKey.format);

	GLuint viewHandle;
	if (viewKey.subresource.firstMipLevel == 0 && viewKey.subresource.numMipLevels == texture->mipLevels &&
	    viewKey.subresource.firstArrayLayer == 0 && viewKey.subresource.numArrayLayers == texture->arrayLayers &&
	    viewKey.type == texture->type && viewKey.format == texture->format)
	{
		viewHandle = texture->texture;
	}
	else
	{
		if (glTextureView == nullptr)
		{
			EG_PANIC("Partial texture views are not supported by this GL context");
		}

		static bool hasWarnedAboutTextureViews = false;
		if (useGLESPath && !hasWarnedAboutTextureViews)
		{
			hasWarnedAboutTextureViews = true;
			eg::Log(
				eg::LogLevel::Warning, "gl",
				"Creating true texture view while running in GLES-preferred mode, this will fail in real GLES.");
		}

#ifndef EG_GLES
		glGenTextures(1, &viewHandle);
		glTextureView(
			viewHandle, viewKey.type, texture->texture, glFormat, viewKey.subresource.firstMipLevel,
			viewKey.subresource.numMipLevels, viewKey.subresource.firstArrayLayer, viewKey.subresource.numArrayLayers);
#endif
	}

	TextureView& view =
		texture->views.emplace(viewKey, TextureView{ viewKey, viewHandle, glFormat, texture }).first->second;
	return reinterpret_cast<TextureViewHandle>(&view);
}

size_t TextureViewKey::Hash() const
{
	size_t h = subresource.Hash();
	HashAppend(h, type);
	HashAppend(h, static_cast<uint32_t>(format));
	return h;
}

bool TextureViewKey::operator==(const TextureViewKey& other) const
{
	return type == other.type && format == other.format && subresource == other.subresource;
}

static std::pair<Format, GLenum> compressedUploadFormats[] = {
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

	static const std::array<GLenum, 5> floatFormats = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };
	static const std::array<GLenum, 5> integerFormats = { 0, GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER,
		                                                  GL_RGBA_INTEGER };
	static const std::array<GLenum, 5> uTypes = { 0, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, 0, GL_UNSIGNED_INT };
	static const std::array<GLenum, 5> sTypes = { 0, GL_BYTE, GL_SHORT, 0, GL_INT };

	switch (GetFormatType(format))
	{
	case FormatTypes::UNorm: return std::make_tuple(floatFormats.at(componentCount), uTypes.at(componentSize));
	case FormatTypes::SNorm: return std::make_tuple(floatFormats.at(componentCount), sTypes.at(componentSize));
	case FormatTypes::UInt: return std::make_tuple(integerFormats.at(componentCount), uTypes.at(componentSize));
	case FormatTypes::SInt: return std::make_tuple(integerFormats.at(componentCount), sTypes.at(componentSize));
	case FormatTypes::Float: return std::make_tuple(floatFormats.at(componentCount), GL_FLOAT);
	case FormatTypes::DepthStencil: EG_PANIC("Attempted to set the texture data for a depth/stencil texture.")
	}

	EG_UNREACHABLE
}

void SetTextureData(
	CommandContextHandle, TextureHandle handle, const TextureRange& range, BufferHandle bufferHandle, uint64_t offset)
{
	AssertRenderPassNotActive("SetTextureData");

	const Buffer* buffer = UnwrapBuffer(bufferHandle);

	char* offsetPtr;
	if (buffer->isFakeHostBuffer)
	{
		offsetPtr = buffer->persistentMapping + offset;
	}
	else
	{
		offsetPtr = reinterpret_cast<char*>(static_cast<uintptr_t>(offset));
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer->buffer);
	}

	Texture* texture = UnwrapTexture(handle);
	auto [format, type] = GetUploadFormat(texture->format);

	texture->ChangeUsage(TextureUsage::CopyDst);

	const bool isCompressed = IsCompressedFormat(texture->format);
	const uint32_t imageBytes = GetImageByteSize(range.sizeX, range.sizeY, texture->format);

	glBindTexture(texture->type, texture->texture);

	if (texture->type == GL_TEXTURE_CUBE_MAP)
	{
		for (int l = 0; l < ToInt(range.sizeZ); l++)
		{
			GLenum glLayer = GL_TEXTURE_CUBE_MAP_POSITIVE_X + l + range.offsetZ;
			char* layerOffsetPtr = offsetPtr + imageBytes * l;
			if (isCompressed)
			{
				glCompressedTexSubImage2D(
					glLayer, range.mipLevel, range.offsetX, range.offsetY, range.sizeX, range.sizeY, format, imageBytes,
					layerOffsetPtr);
			}
			else
			{
				glTexSubImage2D(
					glLayer, range.mipLevel, range.offsetX, range.offsetY, range.sizeX, range.sizeY, format, type,
					layerOffsetPtr);
			}
		}
	}
	else if (texture->dim == 2)
	{
		if (isCompressed)
		{
			glCompressedTexSubImage2D(
				texture->type, range.mipLevel, range.offsetX, range.offsetY, range.sizeX, range.sizeY, format,
				imageBytes, offsetPtr);
		}
		else
		{
			glTexSubImage2D(
				texture->type, range.mipLevel, range.offsetX, range.offsetY, range.sizeX, range.sizeY, format, type,
				offsetPtr);
		}
	}
	else if (texture->dim == 3)
	{
		if (isCompressed)
		{
			glCompressedTexSubImage3D(
				texture->type, range.mipLevel, range.offsetX, range.offsetY, range.offsetZ, range.sizeX, range.sizeY,
				range.sizeZ, format, imageBytes * range.sizeZ, offsetPtr);
		}
		else
		{
			glTexSubImage3D(
				texture->type, range.mipLevel, range.offsetX, range.offsetY, range.offsetZ, range.sizeX, range.sizeY,
				range.sizeZ, format, type, offsetPtr);
		}
	}

	if (!buffer->isFakeHostBuffer)
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
}

void GetTextureData(
	CommandContextHandle, TextureHandle handle, const TextureRange& range, BufferHandle bufferHandle, uint64_t offset)
{
	AssertRenderPassNotActive("GetTextureData");

	const Buffer* buffer = UnwrapBuffer(bufferHandle);

	void* offsetPtr;
	if (buffer->isFakeHostBuffer)
	{
		offsetPtr = buffer->persistentMapping + offset;
	}
	else
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer->buffer);
		offsetPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(offset));
	}

	Texture* texture = UnwrapTexture(handle);
	auto [format, type] = GetUploadFormat(texture->format);

	texture->ChangeUsage(TextureUsage::CopySrc);
	texture->LazyInitializeTextureFBO();

	glBindFramebuffer(GL_READ_FRAMEBUFFER, *texture->fbo);
	glReadPixels(range.offsetX, range.offsetY, range.sizeX, range.sizeY, format, type, offsetPtr);

	if (!buffer->isFakeHostBuffer)
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	}
}

void GenerateMipmaps(CommandContextHandle, TextureHandle handle)
{
	AssertRenderPassNotActive("GenerateMipmaps");
	Texture* texture = UnwrapTexture(handle);
	glBindTexture(texture->type, texture->texture);
	glGenerateMipmap(texture->type);
}

void DestroyTexture(TextureHandle handle)
{
	MainThreadInvoke(
		[texture = UnwrapTexture(handle)]
		{
			for (const auto& view : texture->views)
			{
				if (view.second.handle != texture->texture)
					glDeleteTextures(1, &view.second.handle);
			}
			glDeleteTextures(1, &texture->texture);
			if (texture->fbo)
				glDeleteFramebuffers(1, &*texture->fbo);
			texturePool.Delete(texture);
		});
}

void TextureView::Bind(GLuint sampler, uint32_t glBinding) const
{
	GLESAssertTextureBindNotInCurrentFramebuffer(*texture);

	glBindSampler(glBinding, sampler);
	glActiveTexture(GL_TEXTURE0 + glBinding);
	glBindTexture(key.type, handle);
	/*
	if (useGLESPath)
	{
	    float minLod = static_cast<float>(key.subresource.firstMipLevel);
	    float maxLod = static_cast<float>(key.subresource.firstMipLevel + key.subresource.numMipLevels - 1);
	    if (sampler != 0)
	    {
	        glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, minLod);
	        glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, maxLod);
	    }
	    else
	    {
	        glTexParameterf(key.type, GL_TEXTURE_MIN_LOD, minLod);
	        glTexParameterf(key.type, GL_TEXTURE_MAX_LOD, maxLod);
	    }
	}*/
}

void TextureView::BindAsStorageImage(uint32_t glBinding) const
{
#ifdef EG_GLES
	Log(LogLevel::Error, "gl", "Storage images are not supported");
#else
	glBindImageTexture(glBinding, handle, 0, GL_TRUE, 0, GL_READ_WRITE, glFormat);
#endif
}

void BindTexture(
	CommandContextHandle, TextureViewHandle textureView, SamplerHandle sampler, uint32_t set, uint32_t binding)
{
	UnwrapTextureView(textureView)
		->Bind(UnsignedNarrow<GLuint>(reinterpret_cast<uintptr_t>(sampler)), ResolveBindingForBind(set, binding));
}

void Texture::LazyInitializeTextureFBO()
{
	if (fbo.has_value())
		return;

	GLenum target = GetFormatType(format) == FormatTypes::DepthStencil ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;

	GLuint fboHandle;
	glGenFramebuffers(1, &fboHandle);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fboHandle);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, target, GL_TEXTURE_2D, texture, 0);
	AssertFramebufferComplete(GL_READ_FRAMEBUFFER);
	fbo = fboHandle;
}

void BindStorageImage(CommandContextHandle, TextureViewHandle textureViewHandle, uint32_t set, uint32_t binding)
{
	UnwrapTextureView(textureViewHandle)->BindAsStorageImage(ResolveBindingForBind(set, binding));
}

void CopyTextureData(
	CommandContextHandle, TextureHandle srcHandle, TextureHandle dstHandle, const TextureRange& srcRange,
	const TextureOffset& dstOffset)
{
	AssertRenderPassNotActive("CopyTextureData");

	Texture* srcTex = UnwrapTexture(srcHandle);
	Texture* dstTex = UnwrapTexture(dstHandle);

	if (useGLESPath)
	{
		if (dstTex->type != GL_TEXTURE_2D)
		{
			EG_PANIC("CopyTextureData is only supported for 2D textures in GLES")
		}
		if (srcRange.mipLevel != 0)
		{
			EG_PANIC("CopyTextureData is only supported for source mip level 0 in GLES")
		}

		glBindTexture(dstTex->type, dstTex->texture);

		srcTex->LazyInitializeTextureFBO();

		glBindFramebuffer(GL_READ_FRAMEBUFFER, *srcTex->fbo);
		glCopyTexSubImage2D(
			dstTex->type, dstOffset.mipLevel, dstOffset.offsetX, dstOffset.offsetY, srcRange.offsetX, srcRange.offsetY,
			srcRange.sizeX, srcRange.sizeY);
	}
	else
	{
#ifndef EG_GLES
		glCopyImageSubData(
			srcTex->texture, srcTex->type, srcRange.mipLevel, srcRange.offsetX, srcRange.offsetY, srcRange.offsetZ,
			dstTex->texture, dstTex->type, dstOffset.mipLevel, dstOffset.offsetX, dstOffset.offsetY, dstOffset.offsetZ,
			srcRange.sizeX, srcRange.sizeY, srcRange.sizeZ);
#endif
	}
}

void ClearColorTexture(CommandContextHandle, TextureHandle handle, uint32_t mipLevel, const void* color)
{
	AssertRenderPassNotActive("ClearColorTexture");

	Texture* texture = UnwrapTexture(handle);

	if (useGLESPath)
	{
		texture->LazyInitializeTextureFBO();
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, *texture->fbo);
		glViewport(0, 0, texture->width, texture->height);
		viewportOutOfDate = true;

		switch (GetFormatType(texture->format))
		{
		case FormatTypes::UInt: glClearBufferuiv(GL_COLOR, 0, static_cast<const GLuint*>(color)); break;
		case FormatTypes::SInt: glClearBufferiv(GL_COLOR, 0, static_cast<const GLint*>(color)); break;
		case FormatTypes::SNorm:
		case FormatTypes::UNorm:
		case FormatTypes::Float: glClearBufferfv(GL_COLOR, 0, static_cast<const float*>(color)); break;
		case FormatTypes::DepthStencil: EG_PANIC("Cannot clear DepthStencil image using ClearColorTexture")
		}

		BindCorrectFramebuffer();
	}
	else
	{
#ifndef EG_GLES
		auto [format, type] = GetUploadFormat(texture->format);
		glClearTexImage(texture->texture, mipLevel, format, type, color);
#endif
	}
}

void ResolveTexture(CommandContextHandle, TextureHandle srcHandle, TextureHandle dstHandle, const ResolveRegion& region)
{
	AssertRenderPassNotActive("ResolveTexture");

	Texture* src = UnwrapTexture(srcHandle);
	Texture* dst = UnwrapTexture(dstHandle);

	src->LazyInitializeTextureFBO();
	dst->LazyInitializeTextureFBO();

	GLenum blitBuffer = GL_COLOR_BUFFER_BIT;
	if (GetFormatType(src->format) == FormatTypes::DepthStencil)
		blitBuffer = GL_DEPTH_BUFFER_BIT;

	glBindFramebuffer(GL_READ_FRAMEBUFFER, *src->fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, *dst->fbo);

	glBlitFramebuffer(
		region.srcOffset.x, region.srcOffset.y, region.srcOffset.x + region.width, region.srcOffset.y + region.height,
		region.dstOffset.x, region.dstOffset.y, region.dstOffset.x + region.width, region.dstOffset.y + region.height,
		blitBuffer, GL_NEAREST);

	BindCorrectFramebuffer();
}

inline void MaybeBarrierAfterILS(TextureUsage newUsage)
{
#ifndef EG_GLES
	switch (newUsage)
	{
	case TextureUsage::Undefined: break;
	case TextureUsage::CopySrc:
	case TextureUsage::CopyDst: MaybeInsertBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT); break;
	case TextureUsage::ShaderSample: MaybeInsertBarrier(GL_TEXTURE_FETCH_BARRIER_BIT); break;
	case TextureUsage::FramebufferAttachment: MaybeInsertBarrier(GL_FRAMEBUFFER_BARRIER_BIT); break;
	case TextureUsage::ILSRead:
	case TextureUsage::ILSWrite:
	case TextureUsage::ILSReadWrite: MaybeInsertBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); break;
	}
#endif
}

void TextureBarrier(CommandContextHandle, TextureHandle, const eg::TextureBarrier& barrier)
{
	if (barrier.oldUsage == TextureUsage::ILSWrite || barrier.oldUsage == TextureUsage::ILSReadWrite)
	{
		MaybeBarrierAfterILS(barrier.newUsage);
	}
}

void TextureUsageHint(TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags)
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
} // namespace eg::graphics_api::gl

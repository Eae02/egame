#include "WGPUTexture.hpp"
#include "WGPUBuffer.hpp"
#include "WGPUCommandContext.hpp"
#include "WGPUTranslation.hpp"

namespace eg::graphics_api::webgpu
{
static TextureHandle CreateTexture(
	const TextureCreateInfo& createInfo, WGPUTextureDimension dimension, uint32_t depthOrArrayLayers,
	TextureViewType textureType)
{
	WGPUTextureUsageFlags usage = 0;
	if (HasFlag(createInfo.flags, eg::TextureFlags::CopySrc))
		usage |= WGPUTextureUsage_CopySrc;
	if (HasFlag(createInfo.flags, eg::TextureFlags::CopyDst))
		usage |= WGPUTextureUsage_CopyDst;
	// if (HasFlag(createInfo.flags, eg::TextureFlags::GenerateMipmaps))
	// 	{}
	if (HasFlag(createInfo.flags, eg::TextureFlags::ShaderSample))
		usage |= WGPUTextureUsage_TextureBinding;
	if (HasFlag(createInfo.flags, eg::TextureFlags::StorageImage))
		usage |= WGPUTextureUsage_StorageBinding;
	if (HasFlag(createInfo.flags, eg::TextureFlags::FramebufferAttachment))
		usage |= WGPUTextureUsage_RenderAttachment;

#ifndef __EMSCRIPTEN__
	if (HasFlag(createInfo.flags, eg::TextureFlags::TransientAttachment) &&
	    IsDeviceFeatureEnabled(WGPUFeatureName_TransientAttachments))
	{
		usage |= WGPUTextureUsage_TransientAttachment;
	}
#endif

	WGPUTextureFormat format = TranslateTextureFormat(createInfo.format);

	const WGPUTextureDescriptor textureDesc = {
		.label = createInfo.label,
		.usage = usage,
		.dimension = dimension,
		.size = { 
			.width = createInfo.width,
			.height = createInfo.height,
			.depthOrArrayLayers = depthOrArrayLayers,
		},
		.format = format,
		.mipLevelCount = createInfo.mipLevels,
		.sampleCount = createInfo.sampleCount,
		.viewFormatCount = 1,
		.viewFormats = &format,
	};

	Texture* texture = new Texture;
	texture->texture = wgpuDeviceCreateTexture(wgpuctx.device, &textureDesc);
	texture->format = createInfo.format;
	texture->textureType = textureType;

	return reinterpret_cast<TextureHandle>(texture);
}

TextureHandle CreateTexture2D(const TextureCreateInfo& createInfo)
{
	return CreateTexture(createInfo, WGPUTextureDimension_2D, 1, TextureViewType::Flat2D);
}

TextureHandle CreateTexture2DArray(const TextureCreateInfo& createInfo)
{
	return CreateTexture(createInfo, WGPUTextureDimension_2D, createInfo.arrayLayers, TextureViewType::Array2D);
}

TextureHandle CreateTextureCube(const TextureCreateInfo& createInfo)
{
	EG_ASSERT(createInfo.width == createInfo.height);
	return CreateTexture(createInfo, WGPUTextureDimension_2D, 6, TextureViewType::Cube);
}

TextureHandle CreateTextureCubeArray(const TextureCreateInfo& createInfo)
{
	EG_ASSERT(createInfo.width == createInfo.height);
	return CreateTexture(createInfo, WGPUTextureDimension_2D, 6 * createInfo.arrayLayers, TextureViewType::ArrayCube);
}

TextureHandle CreateTexture3D(const TextureCreateInfo& createInfo)
{
	return CreateTexture(createInfo, WGPUTextureDimension_3D, createInfo.depth, TextureViewType::Flat3D);
}

void DestroyTexture(TextureHandle handle)
{
	Texture* texture = &Texture::Unwrap(handle);
	OnFrameEnd(
		[texture]
		{
			for (const auto& viewEntry : texture->views)
				wgpuTextureViewRelease(viewEntry.second);
			wgpuTextureRelease(texture->texture);
			delete texture;
		});
}

void TextureUsageHint(TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags) {}
void TextureBarrier(CommandContextHandle ctx, TextureHandle handle, const eg::TextureBarrier& barrier) {}

void CopyBufferToTexture(
	CommandContextHandle ctx, TextureHandle handle, const TextureRange& range, BufferHandle buffer,
	const TextureBufferCopyLayout& copyLayout)
{
	CommandContext& wcc = CommandContext::Unwrap(ctx);
	Texture& texture = Texture::Unwrap(handle);

	wcc.EndComputePass();

	EG_ASSERT((copyLayout.rowByteStride % 256) == 0);

	const uint32_t blockSize = GetFormatBlockWidth(texture.format);
	const uint32_t numBlocksY = (range.sizeY + blockSize) / blockSize;

	WGPUImageCopyBuffer imageCopyBuffer = {
		.layout = {
			.offset = copyLayout.offset,
			.bytesPerRow = copyLayout.rowByteStride,
			.rowsPerImage = numBlocksY,
		},
		.buffer = Buffer::Unwrap(buffer).buffer,
	};
	WGPUImageCopyTexture imageCopyTexture = {
		.texture = texture.texture,
		.mipLevel = range.mipLevel,
		.origin = {
			.x = range.offsetX,
			.y = range.offsetY,
			.z = range.offsetZ,
		},
		.aspect = WGPUTextureAspect_All,
	};
	WGPUExtent3D extent = {
		.width = RoundToNextMultiple(range.sizeX, blockSize),
		.height = RoundToNextMultiple(range.sizeY, blockSize),
		.depthOrArrayLayers = range.sizeZ,
	};

	wgpuCommandEncoderCopyBufferToTexture(wcc.encoder, &imageCopyBuffer, &imageCopyTexture, &extent);
}

void CopyTextureToBuffer(
	CommandContextHandle ctx, TextureHandle handle, const TextureRange& range, BufferHandle buffer,
	const TextureBufferCopyLayout& copyLayout)
{
	CommandContext& wcc = CommandContext::Unwrap(ctx);
	Texture& texture = Texture::Unwrap(handle);

	wcc.EndComputePass();

	EG_ASSERT((copyLayout.rowByteStride % 256) == 0);

	const uint32_t blockSize = GetFormatBlockWidth(texture.format);
	const uint32_t numBlocksY = (range.sizeY + blockSize) / blockSize;

	WGPUImageCopyBuffer imageCopyBuffer = {
		.layout = {
			.offset = copyLayout.offset,
			.bytesPerRow = copyLayout.rowByteStride,
			.rowsPerImage = numBlocksY,
		},
		.buffer = Buffer::Unwrap(buffer).buffer,
	};
	WGPUImageCopyTexture imageCopyTexture = {
		.texture = texture.texture,
		.mipLevel = range.mipLevel,
		.origin = {
			.x = range.offsetX,
			.y = range.offsetY,
			.z = range.offsetZ,
		},
		.aspect = WGPUTextureAspect_All,
	};
	WGPUExtent3D extent = {
		.width = RoundToNextMultiple(range.sizeX, blockSize),
		.height = RoundToNextMultiple(range.sizeY, blockSize),
		.depthOrArrayLayers = range.sizeZ,
	};

	wgpuCommandEncoderCopyTextureToBuffer(wcc.encoder, &imageCopyTexture, &imageCopyBuffer, &extent);
}

void CopyTextureData(
	CommandContextHandle ctx, TextureHandle srcHandle, TextureHandle dstHandle, const TextureRange& srcRange,
	const TextureOffset& dstOffset)
{
	CommandContext& wcc = CommandContext::Unwrap(ctx);

	wcc.EndComputePass();

	WGPUImageCopyTexture srcCopy = {
		.texture = Texture::Unwrap(srcHandle).texture,
		.mipLevel = srcRange.mipLevel,
		.origin = {
			.x = srcRange.offsetX,
			.y = srcRange.offsetY,
			.z = srcRange.offsetZ,
		},
		.aspect = WGPUTextureAspect_All,
	};
	WGPUImageCopyTexture dstCopy = {
		.texture = Texture::Unwrap(dstHandle).texture,
		.mipLevel = dstOffset.mipLevel,
		.origin = {
			.x = dstOffset.offsetX,
			.y = dstOffset.offsetY,
			.z = dstOffset.offsetZ,
		},
		.aspect = WGPUTextureAspect_All,
	};
	WGPUExtent3D extent = {
		.width = srcRange.sizeX,
		.height = srcRange.sizeY,
		.depthOrArrayLayers = srcRange.sizeZ,
	};

	wgpuCommandEncoderCopyTextureToTexture(wcc.encoder, &srcCopy, &dstCopy, &extent);
}

void GenerateMipmaps(CommandContextHandle ctx, TextureHandle handle)
{
	// EG_PANIC("Unimplemented: GenerateMipmaps")
}

void BindTexture(CommandContextHandle ctx, TextureViewHandle textureView, uint32_t set, uint32_t binding)
{
	EG_PANIC("Unsupported: BindTexture")
}

void BindSampler(CommandContextHandle, SamplerHandle sampler, uint32_t set, uint32_t binding)
{
	EG_PANIC("Unsupported: BindSampler")
}

void BindStorageImage(CommandContextHandle ctx, TextureViewHandle texture, uint32_t set, uint32_t binding)
{
	EG_PANIC("Unsupported: BindStorageImage")
}

void ResolveTexture(CommandContextHandle ctx, TextureHandle src, TextureHandle dst, const ResolveRegion& region){
	EG_PANIC("Unimplemented: ResolveTexture")
}

TextureSubresource Texture::ResolveSubresourceRem(TextureSubresource subresource) const
{
	if (subresource.numMipLevels == REMAINING_SUBRESOURCE)
	{
		subresource.numMipLevels = wgpuTextureGetMipLevelCount(texture) - subresource.firstMipLevel;
	}

	if (subresource.numArrayLayers == REMAINING_SUBRESOURCE)
	{
		if (textureType == TextureViewType::Flat3D)
			subresource.numArrayLayers = 1;
		else
			subresource.numArrayLayers = wgpuTextureGetDepthOrArrayLayers(texture) - subresource.firstArrayLayer;
	}

	return subresource;
}

WGPUTextureView Texture::GetTextureView(
	std::optional<TextureViewType> viewType, const TextureSubresource& subresource, Format viewFormat)
{
	const TextureViewKey viewKey = {
		.type = viewType.value_or(textureType),
		.format = viewFormat == Format::Undefined ? format : viewFormat,
		.subresource = ResolveSubresourceRem(subresource),
	};

	auto viewIt = views.find(viewKey);
	if (viewIt != views.end())
		return viewIt->second;

	const WGPUTextureViewDescriptor viewDescriptor = {
		.format = TranslateTextureFormat(viewKey.format),
		.dimension = TranslateTextureViewType(viewKey.type),
		.baseMipLevel = viewKey.subresource.firstMipLevel,
		.mipLevelCount = viewKey.subresource.numMipLevels,
		.baseArrayLayer = viewKey.subresource.firstArrayLayer,
		.arrayLayerCount = viewKey.subresource.numArrayLayers,
		.aspect = WGPUTextureAspect_All,
	};

	WGPUTextureView textureView = wgpuTextureCreateView(texture, &viewDescriptor);

	views.emplace(viewKey, textureView);

	return textureView;
}

TextureViewHandle GetTextureView(
	TextureHandle textureHandle, std::optional<TextureViewType> viewType, const TextureSubresource& subresource,
	Format format)
{
	WGPUTextureView view = Texture::Unwrap(textureHandle).GetTextureView(viewType, subresource, format);
	return reinterpret_cast<TextureViewHandle>(view);
}

static inline WGPUAddressMode TranslateSamplerWrapMode(WrapMode mode)
{
	switch (mode)
	{
	case WrapMode::Repeat: return WGPUAddressMode_Repeat;
	case WrapMode::MirroredRepeat: return WGPUAddressMode_MirrorRepeat;
	case WrapMode::ClampToEdge: return WGPUAddressMode_ClampToEdge;
	}
	EG_UNREACHABLE
}

SamplerHandle CreateSampler(const SamplerDescription& description)
{
	WGPUSamplerDescriptor samplerDesc = {
		.addressModeU = TranslateSamplerWrapMode(description.wrapU),
		.addressModeV = TranslateSamplerWrapMode(description.wrapV),
		.addressModeW = TranslateSamplerWrapMode(description.wrapW),
		.magFilter = description.magFilter == TextureFilter::Linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest,
		.minFilter = description.minFilter == TextureFilter::Linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest,
		.mipmapFilter =
			description.mipFilter == TextureFilter::Linear ? WGPUMipmapFilterMode_Linear : WGPUMipmapFilterMode_Nearest,
		.lodMinClamp = description.minLod,
		.lodMaxClamp = description.maxLod,
		.maxAnisotropy = static_cast<uint16_t>(glm::clamp(description.maxAnistropy, 1, UINT16_MAX)),
	};
	if (description.enableCompare)
		samplerDesc.compare = TranslateCompareOp(description.compareOp);

	return reinterpret_cast<SamplerHandle>(wgpuDeviceCreateSampler(wgpuctx.device, &samplerDesc));
}
} // namespace eg::graphics_api::webgpu

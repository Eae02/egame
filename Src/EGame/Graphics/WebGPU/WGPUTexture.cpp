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
	if (HasFlag(createInfo.flags, eg::TextureFlags::GenerateMipmaps))
		EG_PANIC("Unimplemented: TextureFlags::GenerateMipmaps");
	if (HasFlag(createInfo.flags, eg::TextureFlags::ShaderSample))
		usage |= WGPUTextureUsage_TextureBinding;
	if (HasFlag(createInfo.flags, eg::TextureFlags::StorageImage))
		usage |= WGPUTextureUsage_StorageBinding;
	if (HasFlag(createInfo.flags, eg::TextureFlags::FramebufferAttachment))
		usage |= WGPUTextureUsage_RenderAttachment;

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
	Texture& texture = Texture::Unwrap(handle);
	wgpuTextureDestroy(texture.texture);
	delete &texture;
}

void TextureUsageHint(TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags) {}
void TextureBarrier(CommandContextHandle ctx, TextureHandle handle, const eg::TextureBarrier& barrier) {}

void SetTextureData(
	CommandContextHandle ctx, TextureHandle handle, const TextureRange& range, BufferHandle buffer, uint64_t offset)
{
	CommandContext& wcc = CommandContext::Unwrap(ctx);
	Texture& texture = Texture::Unwrap(handle);

	uint32_t rowLen = GetImageByteSize(range.sizeX, 1, texture.format);
	uint32_t layerLen = range.sizeZ > 1 ? GetImageByteSize(range.sizeX, range.sizeY, texture.format) : 0;

	// TODO: Expose there requirements via device limits
	EG_ASSERT(range.sizeY <= 1 || (rowLen % 256) == 0);
	EG_ASSERT(range.sizeZ <= 1 || (layerLen % 256) == 0);

	WGPUImageCopyBuffer imageCopyBuffer = {
		.buffer = Buffer::Unwrap(buffer).buffer,
		.layout = {
			.offset = offset,
			.bytesPerRow = rowLen,
			.rowsPerImage = layerLen,
		},
	};
	WGPUImageCopyTexture imageCopyTexture = {
		.texture = texture.texture,
		.aspect = WGPUTextureAspect_All,
		.mipLevel = range.mipLevel,
		.origin = {
			.x = range.offsetX,
			.y = range.offsetY,
			.z = range.offsetZ,
		},
	};
	WGPUExtent3D extent = {
		.width = range.sizeX,
		.height = range.sizeY,
		.depthOrArrayLayers = range.sizeZ,
	};

	wgpuCommandEncoderCopyBufferToTexture(wcc.encoder, &imageCopyBuffer, &imageCopyTexture, &extent);
}

void GetTextureData(
	CommandContextHandle ctx, TextureHandle handle, const TextureRange& range, BufferHandle buffer, uint64_t offset)
{
	EG_PANIC("Unimplemented: GetTextureData")
}

void CopyTextureData(
	CommandContextHandle ctx, TextureHandle src, TextureHandle dst, const TextureRange& srcRange,
	const TextureOffset& dstOffset)
{
	EG_PANIC("Unimplemented: CopyTextureData")
}

void GenerateMipmaps(CommandContextHandle ctx, TextureHandle handle)
{
	EG_PANIC("Unimplemented: GenerateMipmaps")
}

void BindTexture(
	CommandContextHandle ctx, TextureViewHandle textureView, SamplerHandle sampler, uint32_t set, uint32_t binding)
{
	EG_PANIC("Unimplemented: BindTexture")
}

void BindStorageImage(CommandContextHandle ctx, TextureViewHandle texture, uint32_t set, uint32_t binding)
{
	EG_PANIC("Unimplemented: BindStorageImage")
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

TextureViewHandle GetTextureView(
	TextureHandle textureHandle, TextureViewType viewType, const TextureSubresource& subresource, Format format)
{
	Texture& texture = Texture::Unwrap(textureHandle);

	const TextureViewKey viewKey = {
		.format = format == Format::Undefined ? texture.format : format,
		.type = viewType == TextureViewType::SameAsTexture ? texture.textureType : viewType,
		.subresource = texture.ResolveSubresourceRem(subresource),
	};

	WGPUTextureView textureView;

	auto viewIt = texture.views.find(viewKey);
	if (viewIt != texture.views.end())
	{
		textureView = viewIt->second;
	}
	else
	{
		static const WGPUTextureViewDimension VIEW_DIMENSION_LUT[] = {
			[(uint32_t)TextureViewType::Flat2D] = WGPUTextureViewDimension_2D,
			[(uint32_t)TextureViewType::Flat3D] = WGPUTextureViewDimension_3D,
			[(uint32_t)TextureViewType::Cube] = WGPUTextureViewDimension_Cube,
			[(uint32_t)TextureViewType::Array2D] = WGPUTextureViewDimension_2DArray,
			[(uint32_t)TextureViewType::ArrayCube] = WGPUTextureViewDimension_CubeArray,
		};

		const WGPUTextureViewDescriptor viewDescriptor = {
			.format = TranslateTextureFormat(viewKey.format),
			.dimension = VIEW_DIMENSION_LUT[static_cast<uint32_t>(viewKey.type)],
			.baseMipLevel = viewKey.subresource.firstMipLevel,
			.mipLevelCount = viewKey.subresource.numMipLevels,
			.baseArrayLayer = viewKey.subresource.firstArrayLayer,
			.arrayLayerCount = viewKey.subresource.numArrayLayers,
			.aspect = WGPUTextureAspect_All,
		};

		textureView = wgpuTextureCreateView(texture.texture, &viewDescriptor);

		texture.views.emplace(viewKey, textureView);
	}

	return reinterpret_cast<TextureViewHandle>(textureView);
}

static inline WGPUAddressMode TranslateSamplerWrapMode(WrapMode mode)
{
	switch (mode)
	{
	case WrapMode::Repeat: return WGPUAddressMode_Repeat;
	case WrapMode::MirroredRepeat: return WGPUAddressMode_MirrorRepeat;
	case WrapMode::ClampToEdge: return WGPUAddressMode_ClampToEdge;
	}
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
		.maxAnisotropy = static_cast<uint16_t>(glm::clamp(description.maxAnistropy, 0, UINT16_MAX)),
	};
	if (description.enableCompare)
		samplerDesc.compare = TranslateCompareOp(description.compareOp);

	return reinterpret_cast<SamplerHandle>(wgpuDeviceCreateSampler(wgpuctx.device, &samplerDesc));
}
} // namespace eg::graphics_api::webgpu

#include "MetalTexture.hpp"
#include "../../Assert.hpp"
#include "../Abstraction.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandContext.hpp"
#include "MetalPipeline.hpp"
#include "MetalTranslation.hpp"
#include <Metal/MTLPixelFormat.hpp>
#include <Metal/MTLTexture.hpp>

namespace eg::graphics_api::mtl
{
static MTL::TextureUsage GetTextureUsage(TextureFlags flags)
{
	MTL::TextureUsage usage = {};
	if (HasFlag(flags, TextureFlags::StorageImage))
	{
		// TODO: Maybe add separate flags to TextureFlags for only read / only write
		usage |= MTL::TextureUsageShaderWrite | MTL::TextureUsageShaderRead;
	}
	if (HasFlag(flags, TextureFlags::ShaderSample))
		usage |= MTL::TextureUsageShaderRead;
	if (HasFlag(flags, TextureFlags::FramebufferAttachment))
		usage |= MTL::TextureUsageRenderTarget;
	return usage;
}

static ConcurrentObjectPool<Texture> texturePool;

static TextureHandle CreateTexture(const TextureCreateInfo& createInfo, MTL::TextureType textureType)
{
	Texture* texture = texturePool.New();
	texture->format = createInfo.format;

	MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();

	descriptor->setMipmapLevelCount(createInfo.mipLevels);
	descriptor->setArrayLength(std::max<uint32_t>(createInfo.arrayLayers, 1));
	descriptor->setWidth(createInfo.width);
	descriptor->setHeight(createInfo.height);
	descriptor->setDepth(std::max<uint32_t>(createInfo.depth, 1));
	descriptor->setSampleCount(std::max<uint32_t>(createInfo.sampleCount, 1));
	descriptor->setPixelFormat(TranslatePixelFormat(createInfo.format));
	descriptor->setTextureType(textureType);
	descriptor->setStorageMode(MTL::StorageModePrivate);
	descriptor->setUsage(GetTextureUsage(createInfo.flags));

	if (textureType == MTL::TextureTypeCube || textureType == MTL::TextureTypeCubeArray)
		descriptor->setHeight(createInfo.width);

	texture->texture = metalDevice->newTexture(descriptor);

	if (createInfo.label != nullptr)
		texture->texture->label()->init(createInfo.label, NS::UTF8StringEncoding);

	descriptor->release();

	return reinterpret_cast<TextureHandle>(texture);
}

TextureHandle CreateTexture2D(const TextureCreateInfo& createInfo)
{
	return CreateTexture(createInfo, createInfo.sampleCount == 1 ? MTL::TextureType2D : MTL::TextureType2DMultisample);
}

TextureHandle CreateTexture2DArray(const TextureCreateInfo& createInfo)
{
	return CreateTexture(
		createInfo, createInfo.sampleCount == 1 ? MTL::TextureType2DArray : MTL::TextureType2DMultisampleArray);
}

TextureHandle CreateTexture3D(const TextureCreateInfo& createInfo)
{
	return CreateTexture(createInfo, MTL::TextureType3D);
}

TextureHandle CreateTextureCube(const TextureCreateInfo& createInfo)
{
	return CreateTexture(createInfo, MTL::TextureTypeCube);
}

TextureHandle CreateTextureCubeArray(const TextureCreateInfo& createInfo)
{
	return CreateTexture(createInfo, MTL::TextureTypeCubeArray);
}

void DestroyTexture(TextureHandle handle)
{
	Texture& texture = Texture::Unwrap(handle);
	texture.texture->release();
	for (auto [_, view] : texture.views)
		view->release();
	texturePool.Delete(&texture);
}

void TextureUsageHint(TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags) {}

void TextureBarrier(CommandContextHandle ctx, TextureHandle handle, const eg::TextureBarrier& barrier) {}

void SetTextureData(
	CommandContextHandle cc, TextureHandle texture, const TextureRange& range, BufferHandle buffer, uint64_t offset)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(cc);
	mcc.FlushComputeCommands();

	Texture& mtexture = Texture::Unwrap(texture);

	uint32_t rowLen = GetImageByteSize(range.sizeX, 1, mtexture.format);
	uint32_t layerLen = range.sizeZ > 1 ? GetImageByteSize(range.sizeX, range.sizeY, mtexture.format) : 0;

	uint32_t slice = 0;
	MTL::Size destSize = MTL::Size::Make(range.sizeX, range.sizeY, range.sizeZ);
	MTL::Origin destOrigin = MTL::Origin::Make(range.offsetX, range.offsetY, range.offsetZ);

	if (mtexture.texture->textureType() == MTL::TextureType2DArray) // TODO: Check for other array types
	{
		EG_ASSERT(range.sizeZ == 1);
		slice = range.offsetZ;
		destSize.depth = 1;
		destOrigin.z = 0;
	}

	mcc.GetBlitCmdEncoder().copyFromBuffer(
		UnwrapBuffer(buffer), offset, rowLen, layerLen, destSize, mtexture.texture, slice, range.mipLevel, destOrigin);
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
	// EG_PANIC("Unimplemented: CopyTextureData")
}

void GenerateMipmaps(CommandContextHandle ctx, TextureHandle handle)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	mcc.FlushComputeCommands();
	Texture& mtexture = Texture::Unwrap(handle);
	mcc.GetBlitCmdEncoder().generateMipmaps(mtexture.texture);
}

void BindTexture(
	CommandContextHandle ctx, TextureViewHandle textureView, SamplerHandle sampler, uint32_t set, uint32_t binding)
{
	EG_ASSERT(sampler != nullptr);
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	mcc.BindTexture(UnwrapTextureView(textureView), set, binding);
	mcc.BindSampler(reinterpret_cast<MTL::SamplerState*>(sampler), set, binding);
}

void BindStorageImage(CommandContextHandle ctx, TextureViewHandle texture, uint32_t set, uint32_t binding)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	mcc.BindTexture(UnwrapTextureView(texture), set, binding);
}

void ClearColorTexture(CommandContextHandle ctx, TextureHandle texture, uint32_t mipLevel, const void* color)
{
	EG_PANIC("Unimplemented: ClearColorTexture")
}

void ResolveTexture(CommandContextHandle ctx, TextureHandle src, TextureHandle dst, const ResolveRegion& region)
{
	EG_PANIC("Unimplemented: ResolveTexture")
}

static MTL::TextureType textureViewTypeTranslationTable[] = {
	[(int)TextureViewType::Flat1D] = MTL::TextureType1D,
	[(int)TextureViewType::Flat2D] = MTL::TextureType2D,
	[(int)TextureViewType::Flat3D] = MTL::TextureType3D,
	[(int)TextureViewType::Cube] = MTL::TextureTypeCube,
	[(int)TextureViewType::Array1D] = MTL::TextureType1DArray,
	[(int)TextureViewType::Array2D] = MTL::TextureType2DArray,
	[(int)TextureViewType::ArrayCube] = MTL::TextureTypeCubeArray,
};

size_t TextureViewKey::Hash() const
{
	size_t h = subresource.Hash();
	HashAppend(h, static_cast<uint32_t>(type));
	HashAppend(h, static_cast<uint32_t>(format));
	return h;
}

bool TextureViewKey::operator==(const TextureViewKey& other) const
{
	return type == other.type && format == other.format && subresource == other.subresource;
}

MTL::Texture* Texture::GetTextureView(TextureViewType viewType, const TextureSubresource& subresource, Format format)
{
	TextureViewKey viewKey;

	if (viewType == TextureViewType::SameAsTexture)
	{
		viewKey.type = texture->textureType();
	}
	else
	{
		EG_ASSERT(static_cast<int>(viewType) < std::size(textureViewTypeTranslationTable));
		viewKey.type = textureViewTypeTranslationTable[static_cast<int>(viewType)];
	}

	viewKey.format = format == Format::Undefined ? texture->pixelFormat() : TranslatePixelFormat(format);

	viewKey.subresource = subresource.ResolveRem(texture->mipmapLevelCount(), texture->arrayLength());

	TextureSubresource fullSubresource = {
		.numMipLevels = static_cast<uint32_t>(texture->mipmapLevelCount()),
		.numArrayLayers = static_cast<uint32_t>(texture->arrayLength()),
	};

	if (viewKey.type == texture->textureType() && viewKey.format == texture->pixelFormat() &&
	    viewKey.subresource == fullSubresource)
	{
		return texture;
	}

	auto viewIt = views.find(viewKey);
	if (viewIt != views.end())
		return viewIt->second;

	MTL::Texture* view = texture->newTextureView(
		viewKey.format, viewKey.type,
		NS::Range::Make(viewKey.subresource.firstMipLevel, viewKey.subresource.numMipLevels),
		NS::Range::Make(viewKey.subresource.firstArrayLayer, viewKey.subresource.numArrayLayers));

	views.emplace(viewKey, view);

	return view;
}

TextureViewHandle GetTextureView(
	TextureHandle texture, TextureViewType viewType, const TextureSubresource& subresource, Format format)
{
	return reinterpret_cast<TextureViewHandle>(Texture::Unwrap(texture).GetTextureView(viewType, subresource, format));
}

static inline MTL::SamplerAddressMode TranslateSamplerWrapMode(WrapMode mode)
{
	switch (mode)
	{
	case WrapMode::Repeat: return MTL::SamplerAddressModeRepeat;
	case WrapMode::MirroredRepeat: return MTL::SamplerAddressModeMirrorRepeat;
	case WrapMode::ClampToEdge: return MTL::SamplerAddressModeClampToEdge;
	case WrapMode::ClampToBorder: return MTL::SamplerAddressModeClampToBorderColor;
	}
}

static inline MTL::SamplerBorderColor TranslateBorderColor(BorderColor color)
{
	switch (color)
	{
	case BorderColor::F0000:
	case BorderColor::I0000: return MTL::SamplerBorderColorTransparentBlack;
	case BorderColor::F0001:
	case BorderColor::I0001: return MTL::SamplerBorderColorOpaqueBlack;
	case BorderColor::F1111:
	case BorderColor::I1111: return MTL::SamplerBorderColorOpaqueWhite;
	}
}

std::unordered_map<SamplerDescription, MTL::SamplerState*, MemberFunctionHash<SamplerDescription>> samplersCache;

SamplerHandle CreateSampler(const SamplerDescription& description)
{
	auto samplerIt = samplersCache.find(description);
	if (samplerIt != samplersCache.end())
		return reinterpret_cast<SamplerHandle>(samplerIt->second);

	MTL::SamplerDescriptor* descriptor = MTL::SamplerDescriptor::alloc()->init();

	descriptor->setSAddressMode(TranslateSamplerWrapMode(description.wrapU));
	descriptor->setTAddressMode(TranslateSamplerWrapMode(description.wrapV));
	descriptor->setRAddressMode(TranslateSamplerWrapMode(description.wrapW));

	descriptor->setMinFilter(static_cast<MTL::SamplerMinMagFilter>(description.minFilter == TextureFilter::Linear));
	descriptor->setMagFilter(static_cast<MTL::SamplerMinMagFilter>(description.magFilter == TextureFilter::Linear));

	descriptor->setMipFilter(
		description.mipFilter == TextureFilter::Linear ? MTL::SamplerMipFilterLinear : MTL::SamplerMipFilterNearest);

	descriptor->setMaxAnisotropy(glm::clamp(description.maxAnistropy, 1, 16));

	descriptor->setBorderColor(TranslateBorderColor(description.borderColor));

	descriptor->setSupportArgumentBuffers(true);

	descriptor->setCompareFunction(TranslateCompareOp(description.compareOp));

	MTL::SamplerState* samplerState = metalDevice->newSamplerState(descriptor);
	descriptor->release();

	samplersCache.emplace(description, samplerState);

	return reinterpret_cast<SamplerHandle>(samplerState);
}

void DestroySampler(SamplerHandle handle) {}
} // namespace eg::graphics_api::mtl

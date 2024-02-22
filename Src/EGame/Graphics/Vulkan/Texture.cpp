#ifndef EG_NO_VULKAN
#include "Texture.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "../../Hash.hpp"
#include "../../String.hpp"
#include "../../Utils.hpp"
#include "../Graphics.hpp"
#include "Buffer.hpp"
#include "Pipeline.hpp"
#include "Translation.hpp"
#include "VulkanCommandContext.hpp"

namespace eg::graphics_api::vk
{
static_assert(REMAINING_SUBRESOURCE == VK_REMAINING_MIP_LEVELS);
static_assert(REMAINING_SUBRESOURCE == VK_REMAINING_ARRAY_LAYERS);

ConcurrentObjectPool<Texture> texturePool;

void Texture::Free()
{
	for (const auto& view : views)
		vkDestroyImageView(ctx.device, view.second.view, nullptr);

	vmaDestroyImage(ctx.allocator, image, allocation);

	texturePool.Delete(this);
}

static void InitializeImage(
	Texture& texture, const TextureCreateInfo& createInfo, VkImageType imageType, VkImageViewType viewType,
	const VkExtent3D& extent, uint32_t arrayLayers)
{
	texture.refCount = 1;
	texture.aspectFlags = GetFormatAspect(createInfo.format);
	texture.viewType = viewType;
	texture.numMipLevels = createInfo.mipLevels;
	texture.numArrayLayers = arrayLayers;
	texture.autoBarrier = !HasFlag(createInfo.flags, TextureFlags::ManualBarrier);
	texture.currentUsage = TextureUsage::Undefined;
	texture.currentStageFlags = 0;
	texture.extent = extent;
	texture.sampleCount = std::max(createInfo.sampleCount, 1U);
	texture.originalFormat = createInfo.format;
	texture.format = TranslateFormat(createInfo.format);

	// Creates the image
	VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.extent = extent;
	imageCreateInfo.format = texture.format;
	imageCreateInfo.imageType = imageType;
	imageCreateInfo.samples = static_cast<VkSampleCountFlagBits>(texture.sampleCount);
	imageCreateInfo.mipLevels = createInfo.mipLevels;
	imageCreateInfo.arrayLayers = arrayLayers;

	if (HasFlag(createInfo.flags, TextureFlags::CopySrc) || HasFlag(createInfo.flags, TextureFlags::GenerateMipmaps))
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (HasFlag(createInfo.flags, TextureFlags::CopyDst) || HasFlag(createInfo.flags, TextureFlags::GenerateMipmaps))
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (HasFlag(createInfo.flags, TextureFlags::ShaderSample))
		imageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (HasFlag(createInfo.flags, TextureFlags::StorageImage))
		imageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	if (HasFlag(createInfo.flags, TextureFlags::FramebufferAttachment))
	{
		if (texture.aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
			imageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		else
			imageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}

	if (viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
		imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VmaAllocationCreateInfo allocationCreateInfo = {};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	CheckRes(vmaCreateImage(
		ctx.allocator, &imageCreateInfo, &allocationCreateInfo, &texture.image, &texture.allocation, nullptr));

	if (createInfo.label != nullptr)
	{
		texture.viewLabel = Concat({ createInfo.label, " [View]" });
		SetObjectName(reinterpret_cast<uint64_t>(texture.image), VK_OBJECT_TYPE_IMAGE, createInfo.label);
	}
}

size_t TextureViewKey::Hash() const
{
	size_t h = subresource.Hash();
	HashAppend(h, static_cast<uint32_t>(aspectFlags));
	HashAppend(h, static_cast<uint32_t>(type));
	HashAppend(h, static_cast<uint32_t>(format));
	return h;
}

bool TextureViewKey::operator==(const TextureViewKey& other) const
{
	return aspectFlags == other.aspectFlags && type == other.type && format == other.format &&
	       subresource == other.subresource;
}

TextureView& Texture::GetView(
	const TextureSubresource& subresource, VkImageAspectFlags _aspectFlags,
	std::optional<VkImageViewType> forcedViewType, VkFormat differentFormat)
{
	TextureViewKey viewKey;
	viewKey.aspectFlags = _aspectFlags ? _aspectFlags : aspectFlags;
	viewKey.format = differentFormat == VK_FORMAT_UNDEFINED ? format : differentFormat;
	viewKey.type = forcedViewType.value_or(viewType);
	viewKey.subresource = subresource.ResolveRem(numMipLevels, numArrayLayers);

	auto it = views.find(viewKey);
	if (it != views.end())
		return it->second;

	VkImageViewCreateInfo viewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewCreateInfo.viewType = viewKey.type;
	viewCreateInfo.image = image;
	viewCreateInfo.format = viewKey.format;
	viewCreateInfo.subresourceRange.aspectMask = viewKey.aspectFlags;
	viewCreateInfo.subresourceRange.baseMipLevel = viewKey.subresource.firstMipLevel;
	viewCreateInfo.subresourceRange.levelCount = viewKey.subresource.numMipLevels;
	viewCreateInfo.subresourceRange.baseArrayLayer = viewKey.subresource.firstArrayLayer;
	viewCreateInfo.subresourceRange.layerCount = viewKey.subresource.numArrayLayers;

	VkImageView view;
	CheckRes(vkCreateImageView(ctx.device, &viewCreateInfo, nullptr, &view));

	if (!viewLabel.empty())
	{
		SetObjectName(reinterpret_cast<uint64_t>(view), VK_OBJECT_TYPE_IMAGE_VIEW, viewLabel.c_str());
	}

	return views.emplace(viewKey, TextureView{ view, this }).first->second;
}

static inline std::optional<VkImageViewType> TranslateViewType(TextureViewType viewType)
{
	switch (viewType)
	{
	case TextureViewType::SameAsTexture: return {};
	case TextureViewType::Flat2D: return VK_IMAGE_VIEW_TYPE_2D;
	case TextureViewType::Flat3D: return VK_IMAGE_VIEW_TYPE_3D;
	case TextureViewType::Cube: return VK_IMAGE_VIEW_TYPE_CUBE;
	case TextureViewType::Array2D: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	case TextureViewType::ArrayCube: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	default: EG_UNREACHABLE
	}
}

TextureViewHandle GetTextureView(
	TextureHandle texture, TextureViewType viewType, const TextureSubresource& subresource, Format format)
{
	Texture* tex = UnwrapTexture(texture);
	TextureView& view = tex->GetView(
		subresource, tex->aspectFlags & (~VK_IMAGE_ASPECT_STENCIL_BIT), TranslateViewType(viewType),
		TranslateFormat(format));
	return reinterpret_cast<TextureViewHandle>(&view);
}

inline TextureHandle WrapTexture(Texture* texture)
{
	return reinterpret_cast<TextureHandle>(texture);
}

TextureHandle CreateTexture2D(const TextureCreateInfo& createInfo)
{
	Texture* texture = texturePool.New();

	InitializeImage(
		*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, { createInfo.width, createInfo.height, 1 }, 1);

	return WrapTexture(texture);
}

TextureHandle CreateTexture2DArray(const TextureCreateInfo& createInfo)
{
	Texture* texture = texturePool.New();

	InitializeImage(
		*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY, { createInfo.width, createInfo.height, 1 },
		createInfo.arrayLayers);

	return WrapTexture(texture);
}

TextureHandle CreateTextureCube(const TextureCreateInfo& createInfo)
{
	Texture* texture = texturePool.New();

	InitializeImage(
		*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE, { createInfo.width, createInfo.width, 1 }, 6);

	return WrapTexture(texture);
}

TextureHandle CreateTextureCubeArray(const TextureCreateInfo& createInfo)
{
	Texture* texture = texturePool.New();

	InitializeImage(
		*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
		{ createInfo.width, createInfo.width, 1 }, 6 * createInfo.arrayLayers);

	return WrapTexture(texture);
}

TextureHandle CreateTexture3D(const TextureCreateInfo& createInfo)
{
	Texture* texture = texturePool.New();

	InitializeImage(
		*texture, createInfo, VK_IMAGE_TYPE_3D, VK_IMAGE_VIEW_TYPE_3D,
		{ createInfo.width, createInfo.height, createInfo.depth }, 1);

	return WrapTexture(texture);
}

void DestroyTexture(TextureHandle handle)
{
	UnwrapTexture(handle)->UnRef();
}

inline VkAccessFlags GetBarrierAccess(TextureUsage usage, VkImageAspectFlags aspectFlags)
{
	switch (usage)
	{
	case TextureUsage::Undefined: return 0;
	case TextureUsage::CopySrc: return VK_ACCESS_TRANSFER_READ_BIT;
	case TextureUsage::CopyDst: return VK_ACCESS_TRANSFER_WRITE_BIT;
	case TextureUsage::ShaderSample: return VK_ACCESS_SHADER_READ_BIT;
	case TextureUsage::ILSRead: return VK_ACCESS_SHADER_READ_BIT;
	case TextureUsage::ILSWrite: return VK_ACCESS_SHADER_WRITE_BIT;
	case TextureUsage::ILSReadWrite: return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	case TextureUsage::FramebufferAttachment:
		if (aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
			return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		else
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	EG_UNREACHABLE
}

VkPipelineStageFlags GetBarrierStageFlagsFromUsage(TextureUsage usage, ShaderAccessFlags shaderAccessFlags)
{
	switch (usage)
	{
	case TextureUsage::Undefined: return 0;
	case TextureUsage::CopySrc: return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case TextureUsage::CopyDst: return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case TextureUsage::FramebufferAttachment:
		return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
		       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	case TextureUsage::ILSRead:
	case TextureUsage::ILSWrite:
	case TextureUsage::ILSReadWrite:
	case TextureUsage::ShaderSample: return TranslateShaderPipelineStage(shaderAccessFlags);
	}
	EG_UNREACHABLE
}

VkImageLayout ImageLayoutFromUsage(TextureUsage usage, VkImageAspectFlags aspectFlags)
{
	switch (usage)
	{
	case TextureUsage::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
	case TextureUsage::CopySrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case TextureUsage::CopyDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case TextureUsage::ILSRead: return VK_IMAGE_LAYOUT_GENERAL;
	case TextureUsage::ILSWrite: return VK_IMAGE_LAYOUT_GENERAL;
	case TextureUsage::ILSReadWrite: return VK_IMAGE_LAYOUT_GENERAL;
	case TextureUsage::ShaderSample:
		if (aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		else
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case TextureUsage::FramebufferAttachment:
		if (aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		else
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}
	EG_UNREACHABLE
}

void Texture::AutoBarrier(CommandContextHandle cc, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags)
{
	if (!autoBarrier || currentUsage == newUsage)
		return;

	if (cc != nullptr)
		EG_PANIC("Vulkan resources used on non-direct contexts must use manual barriers");

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.image = image;
	barrier.srcAccessMask = GetBarrierAccess(currentUsage, aspectFlags);
	barrier.dstAccessMask = GetBarrierAccess(newUsage, aspectFlags);
	barrier.oldLayout = ImageLayoutFromUsage(currentUsage, aspectFlags);
	barrier.newLayout = ImageLayoutFromUsage(newUsage, aspectFlags);
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange = { aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	VkPipelineStageFlags dstStageFlags = GetBarrierStageFlagsFromUsage(newUsage, shaderAccessFlags);
	if (currentStageFlags == 0)
		currentStageFlags = dstStageFlags;

	vkCmdPipelineBarrier(
		VulkanCommandContext::currentImmediate->cb, currentStageFlags, dstStageFlags, 0, 0, nullptr, 0, nullptr, 1,
		&barrier);

	currentStageFlags = dstStageFlags;
	currentUsage = newUsage;
}

void TextureBarrier(CommandContextHandle cc, TextureHandle handle, const eg::TextureBarrier& barrier)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Texture* texture = UnwrapTexture(handle);
	vcc.referencedResources.Add(*texture);

	VkImageMemoryBarrier vkBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	vkBarrier.image = texture->image;
	vkBarrier.srcAccessMask = GetBarrierAccess(barrier.oldUsage, texture->aspectFlags);
	vkBarrier.dstAccessMask = GetBarrierAccess(barrier.newUsage, texture->aspectFlags);
	vkBarrier.oldLayout = ImageLayoutFromUsage(barrier.oldUsage, texture->aspectFlags);
	vkBarrier.newLayout = ImageLayoutFromUsage(barrier.newUsage, texture->aspectFlags);
	vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkBarrier.subresourceRange.aspectMask = texture->aspectFlags;
	vkBarrier.subresourceRange.baseMipLevel = barrier.subresource.firstMipLevel;
	vkBarrier.subresourceRange.levelCount = barrier.subresource.numMipLevels;
	vkBarrier.subresourceRange.baseArrayLayer = barrier.subresource.firstArrayLayer;
	vkBarrier.subresourceRange.layerCount = barrier.subresource.numArrayLayers;

	VkPipelineStageFlags srcStageFlags = GetBarrierStageFlagsFromUsage(barrier.oldUsage, barrier.oldAccess);
	VkPipelineStageFlags dstStageFlags = GetBarrierStageFlagsFromUsage(barrier.newUsage, barrier.newAccess);
	if (srcStageFlags == 0)
		srcStageFlags = dstStageFlags;

	vkCmdPipelineBarrier(vcc.cb, srcStageFlags, dstStageFlags, 0, 0, nullptr, 0, nullptr, 1, &vkBarrier);
}

template <typename T>
static inline void InitImageCopyRegion(
	const Texture& texture, const TextureRange& inputRange, const T& inputOffset, VkOffset3D& outOffset,
	VkImageSubresourceLayers& outSubres, VkExtent3D& outExtent)
{
	outOffset.x = inputOffset.offsetX;
	outOffset.y = inputOffset.offsetY;
	outOffset.z = inputOffset.offsetZ;
	outExtent.width = inputRange.sizeX;
	outExtent.height = inputRange.sizeY;
	outExtent.depth = inputRange.sizeZ;
	outSubres.aspectMask = texture.aspectFlags;
	outSubres.mipLevel = inputOffset.mipLevel;

	switch (texture.viewType)
	{
	case VK_IMAGE_VIEW_TYPE_2D:
		outOffset.z = 0;
		outExtent.depth = 1;
		outSubres.baseArrayLayer = 0;
		outSubres.layerCount = 1;
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		outSubres.baseArrayLayer = 0;
		outSubres.layerCount = 1;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		outOffset.z = 0;
		outExtent.depth = 1;
		outSubres.baseArrayLayer = inputRange.offsetZ;
		outSubres.layerCount = inputRange.sizeZ;
		break;
	default: EG_PANIC("Unknown view type encountered in InitImageCopyRegion.");
	}
}

void SetTextureData(
	CommandContextHandle cc, TextureHandle handle, const TextureRange& range, BufferHandle bufferHandle,
	uint64_t offset)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* buffer = UnwrapBuffer(bufferHandle);
	vcc.referencedResources.Add(*buffer);

	Texture* texture = UnwrapTexture(handle);
	vcc.referencedResources.Add(*texture);

	texture->AutoBarrier(cc, TextureUsage::CopyDst);
	buffer->AutoBarrier(cc, BufferUsage::CopySrc);

	VkBufferImageCopy copyRegion = {};
	copyRegion.bufferOffset = offset;
	InitImageCopyRegion(
		*texture, range, range, copyRegion.imageOffset, copyRegion.imageSubresource, copyRegion.imageExtent);

	vkCmdCopyBufferToImage(
		vcc.cb, buffer->buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
}

void GetTextureData(
	CommandContextHandle cc, TextureHandle handle, const TextureRange& range, BufferHandle bufferHandle,
	uint64_t offset)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* buffer = UnwrapBuffer(bufferHandle);
	vcc.referencedResources.Add(*buffer);

	Texture* texture = UnwrapTexture(handle);
	vcc.referencedResources.Add(*texture);

	texture->AutoBarrier(cc, TextureUsage::CopySrc);
	buffer->AutoBarrier(cc, BufferUsage::CopyDst);

	VkBufferImageCopy copyRegion = {};
	copyRegion.bufferOffset = offset;
	InitImageCopyRegion(
		*texture, range, range, copyRegion.imageOffset, copyRegion.imageSubresource, copyRegion.imageExtent);

	vkCmdCopyImageToBuffer(
		vcc.cb, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer->buffer, 1, &copyRegion);
}

void CopyTextureData(
	CommandContextHandle cc, TextureHandle srcHandle, TextureHandle dstHandle, const TextureRange& srcRange,
	const TextureOffset& dstOffset)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Texture* srcTex = UnwrapTexture(srcHandle);
	Texture* dstTex = UnwrapTexture(dstHandle);
	vcc.referencedResources.Add(*srcTex);
	vcc.referencedResources.Add(*dstTex);

	srcTex->AutoBarrier(cc, TextureUsage::CopySrc);
	dstTex->AutoBarrier(cc, TextureUsage::CopyDst);

	VkImageCopy copyRegion = {};
	InitImageCopyRegion(
		*srcTex, srcRange, srcRange, copyRegion.srcOffset, copyRegion.srcSubresource, copyRegion.extent);
	InitImageCopyRegion(
		*dstTex, srcRange, dstOffset, copyRegion.dstOffset, copyRegion.dstSubresource, copyRegion.extent);

	vkCmdCopyImage(
		vcc.cb, srcTex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstTex->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
}

void ClearColorTexture(CommandContextHandle cc, TextureHandle handle, uint32_t mipLevel, const void* color)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Texture* texture = UnwrapTexture(handle);
	vcc.referencedResources.Add(*texture);

	texture->AutoBarrier(cc, TextureUsage::CopyDst);

	VkImageSubresourceRange subresourceRange;
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = mipLevel;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	vkCmdClearColorImage(
		vcc.cb, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, reinterpret_cast<const VkClearColorValue*>(color),
		1, &subresourceRange);
}

void ResolveTexture(
	CommandContextHandle cc, TextureHandle srcHandle, TextureHandle dstHandle, const ResolveRegion& region)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Texture* src = UnwrapTexture(srcHandle);
	Texture* dst = UnwrapTexture(dstHandle);

	vcc.referencedResources.Add(*src);
	vcc.referencedResources.Add(*dst);

	src->AutoBarrier(cc, TextureUsage::CopySrc);
	dst->AutoBarrier(cc, TextureUsage::CopyDst);

	VkImageResolve resolve = {};
	resolve.srcOffset.x = region.srcOffset.x;
	resolve.srcOffset.y = region.srcOffset.y;
	resolve.dstOffset.x = region.dstOffset.x;
	resolve.dstOffset.y = region.dstOffset.y;
	resolve.extent.width = region.width;
	resolve.extent.height = region.height;
	resolve.extent.depth = 1;
	resolve.srcSubresource.aspectMask = src->aspectFlags;
	resolve.srcSubresource.mipLevel = region.srcSubresource.mipLevel;
	resolve.srcSubresource.baseArrayLayer = region.srcSubresource.firstArrayLayer;
	resolve.srcSubresource.layerCount = region.srcSubresource.numArrayLayers;
	resolve.dstSubresource.aspectMask = dst->aspectFlags;
	resolve.dstSubresource.mipLevel = region.dstSubresource.mipLevel;
	resolve.dstSubresource.baseArrayLayer = region.dstSubresource.firstArrayLayer;
	resolve.dstSubresource.layerCount = region.dstSubresource.numArrayLayers;

	vkCmdResolveImage(
		vcc.cb, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
		&resolve);
}

void TextureUsageHint(TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags)
{
	Texture* texture = UnwrapTexture(handle);
	VulkanCommandContext::currentImmediate->referencedResources.Add(*texture);
	texture->AutoBarrier(nullptr, newUsage, shaderAccessFlags);
}

void GenerateMipmaps(CommandContextHandle cc, TextureHandle handle)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Texture* texture = UnwrapTexture(handle);
	vcc.referencedResources.Add(*texture);

	texture->AutoBarrier(cc, TextureUsage::CopyDst);

	VkImageMemoryBarrier preBlitBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	preBlitBarrier.image = texture->image;
	preBlitBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	preBlitBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	preBlitBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	preBlitBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	preBlitBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	preBlitBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	preBlitBarrier.subresourceRange = { texture->aspectFlags, 0, 1, 0, texture->numArrayLayers };

	int srcWidth = texture->extent.width;
	int srcHeight = texture->extent.height;
	for (uint32_t i = 1; i < texture->numMipLevels; i++)
	{
		preBlitBarrier.subresourceRange.baseMipLevel = i - 1;
		vkCmdPipelineBarrier(
			vcc.cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
			&preBlitBarrier);

		int dstWidth = std::max(srcWidth / 2, 1);
		int dstHeight = std::max(srcHeight / 2, 1);

		VkImageBlit blit;
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { srcWidth, srcHeight, 1 };
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { dstWidth, dstHeight, 1 };
		blit.srcSubresource = { texture->aspectFlags, i - 1, 0, texture->numArrayLayers };
		blit.dstSubresource = { texture->aspectFlags, i, 0, texture->numArrayLayers };

		vkCmdBlitImage(
			vcc.cb, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		srcWidth = dstWidth;
		srcHeight = dstHeight;
	}

	preBlitBarrier.subresourceRange.baseMipLevel = texture->numMipLevels - 1;
	vkCmdPipelineBarrier(
		vcc.cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
		&preBlitBarrier);

	texture->currentUsage = TextureUsage::CopySrc;
}
} // namespace eg::graphics_api::vk

#endif

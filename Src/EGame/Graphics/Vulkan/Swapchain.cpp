#include "Swapchain.hpp"
#include "Common.hpp"
#include "EGame/Graphics/Abstraction.hpp"
#include "RenderPasses.hpp"

#include <SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

namespace eg::graphics_api::vk
{
inline VkSurfaceFormatKHR SelectSurfaceFormat(bool useSRGB)
{
	uint32_t numSurfaceFormats;
	vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physDevice, ctx.surface, &numSurfaceFormats, nullptr);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(numSurfaceFormats);
	vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physDevice, ctx.surface, &numSurfaceFormats, surfaceFormats.data());

	// Selects a surface format
	if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { useSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	// Searches for a supported format
	const VkFormat supportedFormats[] = { useSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
		                                  useSRGB ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM,
		                                  useSRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM,
		                                  useSRGB ? VK_FORMAT_B8G8R8_SRGB : VK_FORMAT_B8G8R8_UNORM };

	for (const VkSurfaceFormatKHR& format : surfaceFormats)
	{
		for (VkFormat supportedFormat : supportedFormats)
		{
			if (supportedFormat == format.format)
				return format;
		}
	}

	return { VK_FORMAT_UNDEFINED };
}

inline VkPresentModeKHR SelectPresentMode(bool vSync)
{
	uint32_t numPresentModes;
	vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physDevice, ctx.surface, &numPresentModes, nullptr);
	std::vector<VkPresentModeKHR> presentModes(numPresentModes);
	vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physDevice, ctx.surface, &numPresentModes, presentModes.data());

	auto CanUsePresentMode = [&](VkPresentModeKHR presentMode)
	{ return std::find(presentModes.begin(), presentModes.end(), presentMode) != presentModes.end(); };

	if (!vSync)
	{
		// Try to use immediate present mode if vsync is disabled.
		if (CanUsePresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR))
		{
			Log(LogLevel::Info, "vk", "Selected present mode: immediate");
			return VK_PRESENT_MODE_IMMEDIATE_KHR;
		}

		Log(LogLevel::Warning, "vk",
		    "Disabling V-Sync is not supported by this driver "
		    "(it does not support immediate present mode).");
	}

	if (CanUsePresentMode(VK_PRESENT_MODE_MAILBOX_KHR))
	{
		Log(LogLevel::Info, "vk", "Selected present mode: mailbox");
		return VK_PRESENT_MODE_MAILBOX_KHR;
	}

	if (CanUsePresentMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR))
	{
		Log(LogLevel::Info, "vk", "Selected present mode: fifo_relaxed");
		return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	}

	Log(LogLevel::Info, "vk", "Selected present mode: fifo");
	return VK_PRESENT_MODE_FIFO_KHR;
}

bool Swapchain::Init(const GraphicsAPIInitArguments& initArgs)
{
	m_surfaceFormat = SelectSurfaceFormat(initArgs.defaultFramebufferSRGB);
	if (m_surfaceFormat.format == VK_FORMAT_UNDEFINED)
		return false;
	m_presentMode = SelectPresentMode(true);
	m_window = initArgs.window;
	return true;
}

inline void DestroyDefaultFramebuffer()
{
	if (ctx.defaultDSImage != VK_NULL_HANDLE)
	{
		vkDestroyImageView(ctx.device, ctx.defaultDSImageView, nullptr);
		vmaDestroyImage(ctx.allocator, ctx.defaultDSImage, ctx.defaultDSImageAllocation);
		ctx.defaultDSImage = VK_NULL_HANDLE;
	}

	for (VkFramebuffer& framebuffer : ctx.defaultFramebuffers)
	{
		if (framebuffer != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(ctx.device, framebuffer, nullptr);
			framebuffer = VK_NULL_HANDLE;
		}
	}
}

void Swapchain::Create()
{
	vkQueueWaitIdle(ctx.mainQueue);

	VkSurfaceCapabilitiesKHR capabilities;
	CheckRes(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physDevice, ctx.surface, &capabilities));
	m_surfaceExtent = capabilities.currentExtent;

	if (m_surfaceExtent.width == 0xFFFFFFFF)
	{
		SDL_GetWindowSize(
			m_window, reinterpret_cast<int*>(&m_surfaceExtent.width), reinterpret_cast<int*>(&m_surfaceExtent.height));
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchainCreateInfo.surface = ctx.surface;
	swapchainCreateInfo.minImageCount = std::max<uint32_t>(capabilities.minImageCount, 3);
	swapchainCreateInfo.imageFormat = m_surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = m_surfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = m_surfaceExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.preTransform = capabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = m_presentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapchainCreateInfo.oldSwapchain = m_swapchain;

	vkCreateSwapchainKHR(ctx.device, &swapchainCreateInfo, nullptr, &m_swapchain);

	if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(ctx.device, swapchainCreateInfo.oldSwapchain, nullptr);
	}

	// Fetches swapchain images
	uint32_t numSwapchainImages;
	vkGetSwapchainImagesKHR(ctx.device, m_swapchain, &numSwapchainImages, nullptr);
	m_swapchainImages.resize(numSwapchainImages);
	vkGetSwapchainImagesKHR(ctx.device, m_swapchain, &numSwapchainImages, m_swapchainImages.data());

	// Destroys old swapchain image views
	for (VkImageView imageView : m_swapchainImageViews)
	{
		vkDestroyImageView(ctx.device, imageView, nullptr);
	}

	// Creates new swapchain image views
	VkImageViewCreateInfo viewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = m_surfaceFormat.format;
	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	m_swapchainImageViews.resize(numSwapchainImages);
	for (uint32_t i = 0; i < numSwapchainImages; i++)
	{
		viewCreateInfo.image = m_swapchainImages[i];
		vkCreateImageView(ctx.device, &viewCreateInfo, nullptr, &m_swapchainImageViews[i]);
	}

	// Creates new image semaphores if the number of images has increased
	while (m_acquireSemaphores.size() < numSwapchainImages)
		m_acquireSemaphores.push_back(CreateSemaphore(ctx.device));

	DestroyDefaultFramebuffer();

	RenderPassDescription defaultFBRenderPassDesc;
	defaultFBRenderPassDesc.numColorAttachments = 1;
	defaultFBRenderPassDesc.numResolveColorAttachments = 0;
	defaultFBRenderPassDesc.colorAttachments[0].format = m_surfaceFormat.format;
	defaultFBRenderPassDesc.colorAttachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkImageView attachments[2];

	VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	framebufferCreateInfo.width = m_surfaceExtent.width;
	framebufferCreateInfo.height = m_surfaceExtent.height;
	framebufferCreateInfo.layers = 1;
	framebufferCreateInfo.attachmentCount = 1;
	framebufferCreateInfo.pAttachments = attachments;

	// Creates a new default depth stencil image and view
	if (ctx.defaultDSFormat != VK_FORMAT_UNDEFINED)
	{
		// Creates the image
		VkImageCreateInfo dsImageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		dsImageCreateInfo.extent = { m_surfaceExtent.width, m_surfaceExtent.height, 1 };
		dsImageCreateInfo.format = ctx.defaultDSFormat;
		dsImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		dsImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		dsImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		dsImageCreateInfo.mipLevels = 1;
		dsImageCreateInfo.arrayLayers = 1;

		VmaAllocationCreateInfo allocationCreateInfo = {};
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		CheckRes(vmaCreateImage(
			ctx.allocator, &dsImageCreateInfo, &allocationCreateInfo, &ctx.defaultDSImage,
			&ctx.defaultDSImageAllocation, nullptr));

		// Creates the image view
		VkImageViewCreateInfo dsImageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		dsImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		dsImageViewCreateInfo.format = ctx.defaultDSFormat;
		dsImageViewCreateInfo.image = ctx.defaultDSImage;
		dsImageViewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
		CheckRes(vkCreateImageView(ctx.device, &dsImageViewCreateInfo, nullptr, &ctx.defaultDSImageView));
		if (HasStencil(ctx.defaultDSFormat))
			dsImageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		SetObjectName(std::bit_cast<uint64_t>(ctx.defaultDSImage), VK_OBJECT_TYPE_IMAGE, "Default DepthStencil");
		SetObjectName(
			std::bit_cast<uint64_t>(ctx.defaultDSImageView), VK_OBJECT_TYPE_IMAGE_VIEW, "Default DepthStencil View");

		defaultFBRenderPassDesc.depthAttachment.format = ctx.defaultDSFormat;
		defaultFBRenderPassDesc.depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		framebufferCreateInfo.attachmentCount = 2;
		attachments[0] = ctx.defaultDSImageView;
	}

	framebufferCreateInfo.renderPass = GetRenderPass(defaultFBRenderPassDesc, true);

	for (uint32_t i = 0; i < numSwapchainImages; i++)
	{
		attachments[framebufferCreateInfo.attachmentCount - 1] = m_swapchainImageViews[i];
		CheckRes(vkCreateFramebuffer(ctx.device, &framebufferCreateInfo, nullptr, &ctx.defaultFramebuffers[i]));
		SetObjectName(
			std::bit_cast<uint64_t>(ctx.defaultFramebuffers[i]), VK_OBJECT_TYPE_FRAMEBUFFER, "Default Framebuffer");
	}

	m_acquireSemaphoreIndex = 0;
}

void Swapchain::SetEnableVSync(bool enableVSync)
{
	m_presentMode = SelectPresentMode(enableVSync);
	Create();
}

VkSemaphore Swapchain::AcquireImage()
{
	VkResult result = vkAcquireNextImageKHR(
		ctx.device, m_swapchain, UINT64_MAX, m_acquireSemaphores[m_acquireSemaphoreIndex], VK_NULL_HANDLE,
		&m_currentImage);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		Create();
		return AcquireImage();
	}
	else
	{
		CheckRes(result);
		VkSemaphore acquireSemaphore = m_acquireSemaphores[m_acquireSemaphoreIndex];
		m_acquireSemaphoreIndex = (m_acquireSemaphoreIndex + 1) % NumImages();
		return acquireSemaphore;
	}
}

void Swapchain::Destroy()
{
	DestroyDefaultFramebuffer();

	for (VkSemaphore aquireSemaphore : m_acquireSemaphores)
	{
		if (aquireSemaphore != VK_NULL_HANDLE)
			vkDestroySemaphore(ctx.device, aquireSemaphore, nullptr);
	}

	for (VkImageView swView : m_swapchainImageViews)
	{
		if (swView != VK_NULL_HANDLE)
			vkDestroyImageView(ctx.device, swView, nullptr);
	}

	vkDestroySwapchainKHR(ctx.device, m_swapchain, nullptr);
	m_swapchain = VK_NULL_HANDLE;
}
} // namespace eg::graphics_api::vk

#include "VulkanMain.hpp"
#include "Common.hpp"
#include "Sampler.hpp"
#include "RenderPasses.hpp"
#include "../../Core.hpp"

#include <SDL2/SDL_vulkan.h>
#include <volk.h>

namespace eg::graphics_api::vk
{
	Context ctx;
	
	inline VkSurfaceFormatKHR SelectSurfaceFormat(bool useSRGB)
	{
		uint32_t numSurfaceFormats;
		vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physDevice, ctx.surface, &numSurfaceFormats, nullptr);
		std::vector<VkSurfaceFormatKHR> surfaceFormats(numSurfaceFormats);
		vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physDevice, ctx.surface, &numSurfaceFormats, surfaceFormats.data());
		
		//Selects a surface format
		if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			return { useSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}
		
		//Searches for a supported format
		const VkFormat supportedFormats[] =
		{
			useSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
			useSRGB ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM,
			useSRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM,
			useSRGB ? VK_FORMAT_B8G8R8_SRGB : VK_FORMAT_B8G8R8_UNORM
		};
		
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
		
		auto CanUsePresentMode = [&] (VkPresentModeKHR presentMode)
		{
			return std::find(presentModes.begin(), presentModes.end(), presentMode) != presentModes.end();
		};
		
		if (!vSync)
		{
			//Try to use immediate present mode if vsync is disabled.
			if (CanUsePresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR))
			{
				Log(LogLevel::Info, "vk", "Selected present mode: immediate");
				return VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
			
			Log(LogLevel::Warning, "vk", "Disabling V-Sync is not supported by this driver "
				"(it does not support immediate present mode).");
		}
		
		if (CanUsePresentMode(VK_PRESENT_MODE_MAILBOX_KHR))
		{
			Log(LogLevel::Info, "vk", "Selected present mode: mailbox");
			return VK_PRESENT_MODE_MAILBOX_KHR;
		}
		
		Log(LogLevel::Info, "vk", "Selected present mode: fifo");
		return VK_PRESENT_MODE_FIFO_KHR;
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
	
	static void CreateSwapchain()
	{
		vkQueueWaitIdle(ctx.mainQueue);
		
		VkSurfaceCapabilitiesKHR capabilities;
		CheckRes(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physDevice, ctx.surface, &capabilities));
		ctx.surfaceExtent = capabilities.currentExtent;
		
		VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		swapchainCreateInfo.surface = ctx.surface;
		swapchainCreateInfo.minImageCount = std::max<uint32_t>(capabilities.minImageCount, 3);
		swapchainCreateInfo.imageFormat = ctx.surfaceFormat.format;
		swapchainCreateInfo.imageColorSpace = ctx.surfaceFormat.colorSpace;
		swapchainCreateInfo.imageExtent = capabilities.currentExtent;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.preTransform = capabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.presentMode = ctx.presentMode;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		swapchainCreateInfo.oldSwapchain = ctx.swapchain;
		
		vkCreateSwapchainKHR(ctx.device, &swapchainCreateInfo, nullptr, &ctx.swapchain);
		
		if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(ctx.device, swapchainCreateInfo.oldSwapchain, nullptr);
		}
		
		//Fetches swapchain images
		uint32_t numSwapchainImages;
		vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &numSwapchainImages, nullptr);
		if (ctx.numSwapchainImages > 16)
		{
			EG_PANIC("Too many swapchain images!");
		}
		vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &numSwapchainImages, ctx.swapchainImages);
		
		//Destroys old swapchain image views
		for (uint32_t i = 0; i < ctx.numSwapchainImages; i++)
		{
			vkDestroyImageView(ctx.device, ctx.swapchainImageViews[i], nullptr);
		}
		
		//Creates new swapchain image views
		VkImageViewCreateInfo viewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = ctx.surfaceFormat.format;
		viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		for (uint32_t i = 0; i < numSwapchainImages; i++)
		{
			viewCreateInfo.image = ctx.swapchainImages[i];
			vkCreateImageView(ctx.device, &viewCreateInfo, nullptr, &ctx.swapchainImageViews[i]);
		}
		
		//Creates new image semaphores if the number of images as increased
		for (uint32_t i = ctx.numSwapchainImages; i < numSwapchainImages; i++)
		{
			ctx.acquireSemaphores[i] = CreateSemaphore(ctx.device);
		}
		
		DestroyDefaultFramebuffer();
		
		RenderPassDescription defaultFBRenderPassDesc;
		defaultFBRenderPassDesc.colorAttachments[0].format = ctx.surfaceFormat.format;
		
		VkImageView attachments[2];
		
		VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferCreateInfo.width = ctx.surfaceExtent.width;
		framebufferCreateInfo.height = ctx.surfaceExtent.height;
		framebufferCreateInfo.layers = 1;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = attachments;
		
		//Creates a new default depth stencil image and view
		if (ctx.defaultDSFormat != VK_FORMAT_UNDEFINED)
		{
			//Creates the image
			VkImageCreateInfo dsImageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			dsImageCreateInfo.extent = { ctx.surfaceExtent.width, ctx.surfaceExtent.height, 1 };
			dsImageCreateInfo.format = ctx.defaultDSFormat;
			dsImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			dsImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			dsImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			dsImageCreateInfo.mipLevels = 1;
			dsImageCreateInfo.arrayLayers = 1;
			
			VmaAllocationCreateInfo allocationCreateInfo = { };
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
			CheckRes(vmaCreateImage(ctx.allocator, &dsImageCreateInfo, &allocationCreateInfo, &ctx.defaultDSImage,
				&ctx.defaultDSImageAllocation, nullptr));
			
			//Creates the image view
			VkImageViewCreateInfo dsImageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			dsImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			dsImageViewCreateInfo.format = ctx.defaultDSFormat;
			dsImageViewCreateInfo.image = ctx.defaultDSImage;
			dsImageViewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
			CheckRes(vkCreateImageView(ctx.device, &dsImageViewCreateInfo, nullptr, &ctx.defaultDSImageView));
			if (HasStencil(ctx.defaultDSFormat))
				dsImageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			
			SetObjectName((uint64_t)ctx.defaultDSImage, VK_OBJECT_TYPE_IMAGE, "Default DepthStencil");
			SetObjectName((uint64_t)ctx.defaultDSImageView, VK_OBJECT_TYPE_IMAGE_VIEW, "Default DepthStencil View");
			
			defaultFBRenderPassDesc.depthAttachment.format = ctx.defaultDSFormat;
			framebufferCreateInfo.attachmentCount = 2;
			attachments[0] = ctx.defaultDSImageView;
		}
		
		framebufferCreateInfo.renderPass = GetRenderPass(defaultFBRenderPassDesc, true);
		
		for (uint32_t i = 0; i < numSwapchainImages; i++)
		{
			attachments[framebufferCreateInfo.attachmentCount - 1] = ctx.swapchainImageViews[i];
			CheckRes(vkCreateFramebuffer(ctx.device, &framebufferCreateInfo, nullptr, &ctx.defaultFramebuffers[i]));
			SetObjectName((uint64_t)ctx.defaultFramebuffers[i], VK_OBJECT_TYPE_FRAMEBUFFER, "Default Framebuffer");
		}
		
		ctx.acquireSemaphoreIndex = 0;
		ctx.numSwapchainImages = numSwapchainImages;
	}
	
	bool Initialize(const GraphicsAPIInitArguments& initArguments)
	{
		if (volkInitialize() != VK_SUCCESS)
			return false;
		
		//Enumerates supported instance extensions
		uint32_t availInstanceExtensions;
		vkEnumerateInstanceExtensionProperties(nullptr, &availInstanceExtensions, nullptr);
		std::vector<VkExtensionProperties> extensionProperties(availInstanceExtensions);
		vkEnumerateInstanceExtensionProperties(nullptr, &availInstanceExtensions, extensionProperties.data());
		auto InstanceExtensionSupported = [&](const char* name)
		{
			for (VkExtensionProperties& extension : extensionProperties)
			{
				if (std::strcmp(extension.extensionName, name) == 0)
					return true;
			}
			return false;
		};
		
		//Selects instance extensions
		uint32_t sdlNumInstanceExtensions;
		SDL_Vulkan_GetInstanceExtensions(initArguments.window, &sdlNumInstanceExtensions, nullptr);
		std::vector<const char*> instanceExtensions(sdlNumInstanceExtensions);
		SDL_Vulkan_GetInstanceExtensions(initArguments.window, &sdlNumInstanceExtensions, instanceExtensions.data());
		
		if (!InstanceExtensionSupported(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
		{
			Log(LogLevel::Error, "vk", "Vulkan failed to initialize because required instance extension "
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME " is not available.");
			return false;
		}
		instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		
		ctx.hasDebugUtils = false;
		if (DevMode() && InstanceExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
		{
			ctx.hasDebugUtils = true;
			instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		
		VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		applicationInfo.pApplicationName = GameName().c_str();
		applicationInfo.pEngineName = "EGame";
		applicationInfo.apiVersion = VK_API_VERSION_1_0;
		
		//Creates the instance
		const char* standardValidationLayerName = "VK_LAYER_LUNARG_standard_validation";
		const VkInstanceCreateInfo instanceCreateInfo =
		{
			/* sType                   */ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			/* pNext                   */ nullptr,
			/* flags                   */ 0,
			/* pApplicationInfo        */ &applicationInfo,
			/* enabledLayerCount       */ (uint32_t)(DevMode() ? 1 : 0),
			/* ppEnabledLayerNames     */ &standardValidationLayerName,
			/* enabledExtensionCount   */ (uint32_t)instanceExtensions.size(),
			/* ppEnabledExtensionNames */ instanceExtensions.data()
		};
		VkResult instanceCreateRes = vkCreateInstance(&instanceCreateInfo, nullptr, &ctx.instance);
		if (instanceCreateRes != VK_SUCCESS)
		{
			Log(LogLevel::Error, "vk", "Vulkan instance creation failed with status: {0}", instanceCreateRes);
			return false;
		}
		
		volkLoadInstance(ctx.instance);
		
		if (!SDL_Vulkan_CreateSurface(initArguments.window, ctx.instance, &ctx.surface))
		{
			Log(LogLevel::Error, "gfx", "Vulkan surface creation failed: {0}", SDL_GetError());
			return false;
		}
		
		//Enumerates physical devices
		uint32_t numDevices;
		vkEnumeratePhysicalDevices(ctx.instance, &numDevices, nullptr);
		std::vector<VkPhysicalDevice> physicalDevices(numDevices);
		vkEnumeratePhysicalDevices(ctx.instance, &numDevices, physicalDevices.data());
		
		//Selects which physical device to use
		bool hasExtGetMemoryRequirements2 = false;
		bool hasExtDedicatedAllocation = false;
		bool supportsMultipleGraphicsQueues = false;
		for (VkPhysicalDevice physicalDevice : physicalDevices)
		{
			vkGetPhysicalDeviceFeatures(physicalDevice, &ctx.deviceFeatures);
			
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
			ctx.deviceLimits = deviceProperties.limits;
			
			//Enumerates the queue families exposed by this device
			uint32_t numQueueFamilies;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, queueFamilyProperties.data());
			
			//Searches for a compatible queue family
			const int REQUIRED_QUEUE_FLAGS = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
			bool foundQueueFamily = false;
			for (uint32_t i = 0; i < numQueueFamilies; i++)
			{
				VkBool32 surfaceSupported;
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, ctx.surface, &surfaceSupported);
				if (!surfaceSupported)
					continue;
				
				if ((queueFamilyProperties[i].queueFlags & REQUIRED_QUEUE_FLAGS) == REQUIRED_QUEUE_FLAGS)
				{
					ctx.queueFamily = i;
					ctx.queueFamilyProperties = queueFamilyProperties[i];
					supportsMultipleGraphicsQueues = queueFamilyProperties[i].queueCount > 1;
					foundQueueFamily = true;
					break;
				}
			}
			
			if (!foundQueueFamily)
			{
				Log(LogLevel::Info, "vk", "Cannot use vulkan device '{0}' because it does not have a queue family "
					" that supports graphics, compute and present.", deviceProperties.deviceName);
				continue;
			}
			
			//Enumerates supported device extensions
			uint32_t availDevExtensions;
			vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availDevExtensions, nullptr);
			std::vector<VkExtensionProperties> devExtensionProperties(availDevExtensions);
			vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availDevExtensions,
			                                     devExtensionProperties.data());
			
			//Checks which device extensions are supported
			bool hasExtSwapchain = false;
			bool hasExtPushDescriptor = false;
			bool hasExtMaintenance1 = false;
			hasExtGetMemoryRequirements2 = false;
			hasExtDedicatedAllocation = false;
			for (const VkExtensionProperties& extProperties : devExtensionProperties)
			{
				if (std::strcmp(extProperties.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
					hasExtSwapchain = true;
				else if (std::strcmp(extProperties.extensionName, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME) == 0)
					hasExtPushDescriptor = true;
				else if (std::strcmp(extProperties.extensionName, VK_KHR_MAINTENANCE1_EXTENSION_NAME) == 0)
					hasExtMaintenance1 = true;
				else if (std::strcmp(extProperties.extensionName, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
					hasExtGetMemoryRequirements2 = true;
				else if (std::strcmp(extProperties.extensionName, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
					hasExtDedicatedAllocation = true;
			}
			
			if (!hasExtSwapchain)
			{
				Log(LogLevel::Info, "vk", "Cannot use vulkan device '{0}' because it does not support the "
					VK_KHR_SWAPCHAIN_EXTENSION_NAME " extension", deviceProperties.deviceName);
				continue;
			}
			
			if (!hasExtPushDescriptor)
			{
				Log(LogLevel::Info, "vk", "Cannot use vulkan device '{0}' because it does not support the "
					VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME " extension", deviceProperties.deviceName);
				continue;
			}
			
			if (!hasExtMaintenance1)
			{
				Log(LogLevel::Info, "vk", "Cannot use vulkan device '{0}' because it does not support the "
					VK_KHR_MAINTENANCE1_EXTENSION_NAME " extension", deviceProperties.deviceName);
				continue;
			}
			
			ctx.physDevice = physicalDevice;
			
			Log(LogLevel::Info, "vk", "Using vulkan device: '{0}'", deviceProperties.deviceName);
			
			break;
		}
		
		if (ctx.physDevice == VK_NULL_HANDLE)
		{
			Log(LogLevel::Error, "vk", "No compatible vulkan device was found");
			return false;
		}
		
		vkGetPhysicalDeviceMemoryProperties(ctx.physDevice, &ctx.memoryProperties);
		
		//Creates the logical device
		const float queuePriorities[] = {1, 1};
		VkDeviceQueueCreateInfo queueCreateInfo =
		{
			/* sType            */ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			/* pNext            */ nullptr,
			/* flags            */ 0,
			/* queueFamilyIndex */ ctx.queueFamily,
			/* queueCount       */ static_cast<uint32_t>(supportsMultipleGraphicsQueues ? 2 : 1),
			/* pQueuePriorities */ queuePriorities
		};
		
		VkPhysicalDeviceFeatures enabledDeviceFeatures = {};
		if (DevMode() && ctx.deviceFeatures.robustBufferAccess)
			enabledDeviceFeatures.robustBufferAccess = VK_TRUE;
		enabledDeviceFeatures.samplerAnisotropy = ctx.deviceFeatures.samplerAnisotropy;
		enabledDeviceFeatures.fillModeNonSolid = ctx.deviceFeatures.fillModeNonSolid;
		
		uint32_t numEnabledDeviceExtensions = 0;
		const char* enabledDeviceExtensions[16];
		enabledDeviceExtensions[numEnabledDeviceExtensions++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		enabledDeviceExtensions[numEnabledDeviceExtensions++] = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
		enabledDeviceExtensions[numEnabledDeviceExtensions++] = VK_KHR_MAINTENANCE1_EXTENSION_NAME;
		
		const bool hasDedicatedAllocation = hasExtDedicatedAllocation && hasExtGetMemoryRequirements2;
		if (hasDedicatedAllocation)
		{
			enabledDeviceExtensions[numEnabledDeviceExtensions++] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
			enabledDeviceExtensions[numEnabledDeviceExtensions++] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
		}
		
		VkDeviceCreateInfo deviceCreateInfo =
		{
			/* sType                   */ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			/* pNext                   */ nullptr,
			/* flags                   */ 0,
			/* queueCreateInfoCount    */ 1,
			/* pQueueCreateInfos       */ &queueCreateInfo,
			/* enabledLayerCount       */ 0,
			/* ppEnabledLayerNames     */ nullptr,
			/* enabledExtensionCount   */ numEnabledDeviceExtensions,
			/* ppEnabledExtensionNames */ enabledDeviceExtensions,
			/* pEnabledFeatures        */ &enabledDeviceFeatures
		};
		
		VkResult createDeviceRes = vkCreateDevice(ctx.physDevice, &deviceCreateInfo, nullptr, &ctx.device);
		if (createDeviceRes != VK_SUCCESS)
		{
			Log(LogLevel::Error, "vk", "Vulkan device creation failed with status: {0}", createDeviceRes);
			return false;
		}
		
		volkLoadDevice(ctx.device);
		
		//Gets queue handles
		vkGetDeviceQueue(ctx.device, ctx.queueFamily, 0, &ctx.mainQueue);
		if (supportsMultipleGraphicsQueues)
		{
			vkGetDeviceQueue(ctx.device, ctx.queueFamily, 1, &ctx.backgroundQueue);
		}
		else
		{
			ctx.backgroundQueue = ctx.mainQueue;
		}
		
		//Creates the main command pool
		const VkCommandPoolCreateInfo mainCommandPoolCreateInfo =
		{
			/* sType            */ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			/* pNext            */ nullptr,
			/* flags            */ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			/* queueFamilyIndex */ ctx.queueFamily
		};
		CheckRes(vkCreateCommandPool(ctx.device, &mainCommandPoolCreateInfo, nullptr, &ctx.mainCommandPool));
		
		//Creates the debug messenger
		if (ctx.hasDebugUtils)
		{
			const VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo =
			{
				/* sType           */ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				/* pNext           */ nullptr,
				/* flags           */ 0,
				/* messageSeverity */ VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
				/* messageType     */ VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
				/* pfnUserCallback */ DebugCallback,
				/* pUserData       */ nullptr
			};
			CheckRes(vkCreateDebugUtilsMessengerEXT(ctx.instance, &messengerCreateInfo, nullptr, &ctx.debugMessenger));
		}
		
		VmaVulkanFunctions allocatorVulkanFunctions = { };
		allocatorVulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
		allocatorVulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
		allocatorVulkanFunctions.vkAllocateMemory = vkAllocateMemory;
		allocatorVulkanFunctions.vkFreeMemory = vkFreeMemory;
		allocatorVulkanFunctions.vkMapMemory = vkMapMemory;
		allocatorVulkanFunctions.vkUnmapMemory = vkUnmapMemory;
		allocatorVulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
		allocatorVulkanFunctions.vkBindImageMemory = vkBindImageMemory;
		allocatorVulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
		allocatorVulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
		allocatorVulkanFunctions.vkCreateBuffer = vkCreateBuffer;
		allocatorVulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
		allocatorVulkanFunctions.vkCreateImage = vkCreateImage;
		allocatorVulkanFunctions.vkDestroyImage = vkDestroyImage;
		
		VmaAllocatorCreateInfo allocatorCreateInfo = { };
		allocatorCreateInfo.physicalDevice = ctx.physDevice;
		allocatorCreateInfo.device = ctx.device;
		allocatorCreateInfo.pVulkanFunctions = &allocatorVulkanFunctions;
		
		if (hasDedicatedAllocation)
		{
			allocatorVulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
			allocatorVulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
			allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		}
		CheckRes(vmaCreateAllocator(&allocatorCreateInfo, &ctx.allocator));
		
		ctx.surfaceFormat = SelectSurfaceFormat(initArguments.defaultFramebufferSRGB);
		if (ctx.surfaceFormat.format == VK_FORMAT_UNDEFINED)
			return false;
		
		ctx.presentMode = SelectPresentMode(initArguments.enableVSync);
		ctx.defaultDSFormat = TranslateFormat(initArguments.defaultDepthStencilFormat);
		CreateSwapchain();
		
		//Creates frame queue resources
		VkFenceCreateInfo frameQueueFenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			frameQueueFenceCreateInfo.flags = i == 0 ? 0 : VK_FENCE_CREATE_SIGNALED_BIT;
			CheckRes(vkCreateFence(ctx.device, &frameQueueFenceCreateInfo, nullptr, &ctx.frameQueueFences[i]));
			ctx.frameQueueSemaphores[i] = CreateSemaphore(ctx.device);
		}
		
		//Allocates immediate command buffers
		VkCommandBufferAllocateInfo cmdAllocateInfo = 
		{
			/* sType              */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			/* pNext              */ nullptr,
			/* commandPool        */ ctx.mainCommandPool,
			/* level              */ VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			/* commandBufferCount */ MAX_CONCURRENT_FRAMES
		};
		CheckRes(vkAllocateCommandBuffers(ctx.device, &cmdAllocateInfo, ctx.immediateCommandBuffers));
		
		//Starts the first immediate command buffer
		static const VkCommandBufferBeginInfo beginInfo = 
		{
			/* sType            */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			/* pNext            */ nullptr,
			/* flags            */ VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			/* pInheritanceInfo */ nullptr
		};
		vkBeginCommandBuffer(ctx.immediateCommandBuffers[0], &beginInfo);
		
		return true;
	}
	
	void GetCapabilities(GraphicsCapabilities& capabilities)
	{
		capabilities.uniformBufferAlignment = ctx.deviceLimits.minUniformBufferOffsetAlignment;
		capabilities.depthRange = DepthRange::ZeroToOne;
	}
	
	void DestroyCachedDescriptorSets();
	
	void Shutdown()
	{
		vkDeviceWaitIdle(ctx.device);
		
		DestroyCachedDescriptorSets();
		DestroySamplers();
		DestroyRenderPasses();
		DestroyDefaultFramebuffer();
		
		for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			ctx.referencedResources[i].Release();
			vkDestroyFence(ctx.device, ctx.frameQueueFences[i], nullptr);
			vkDestroySemaphore(ctx.device, ctx.frameQueueSemaphores[i], nullptr);
		}
		
		vkDestroyCommandPool(ctx.device, ctx.mainCommandPool, nullptr);
		
		for (VkSemaphore aquireSemaphore : ctx.acquireSemaphores)
		{
			if (aquireSemaphore != VK_NULL_HANDLE)
				vkDestroySemaphore(ctx.device, aquireSemaphore, nullptr);
		}
		
		for (VkImageView swView : ctx.swapchainImageViews)
		{
			if (swView != VK_NULL_HANDLE)
				vkDestroyImageView(ctx.device, swView, nullptr);
		}
		
		vkDestroySwapchainKHR(ctx.device, ctx.swapchain, nullptr);
		
		vmaDestroyAllocator(ctx.allocator);
		
		vkDestroyDevice(ctx.device, nullptr);
		
		vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
		
		if (ctx.debugMessenger)
			vkDestroyDebugUtilsMessengerEXT(ctx.instance, ctx.debugMessenger, nullptr);
		
		vkDestroyInstance(ctx.instance, nullptr);
	}
	
	void EndLoading()
	{
		vkEndCommandBuffer(ctx.immediateCommandBuffers[0]);
		
		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &ctx.immediateCommandBuffers[0];
		
		vkQueueSubmit(ctx.mainQueue, 1, &submitInfo, ctx.frameQueueFences[0]);
	}
	
	bool IsLoadingComplete()
	{
		return vkGetFenceStatus(ctx.device, ctx.frameQueueFences[0]) == VK_SUCCESS;
	}
	
	static VkSemaphore acquireSemaphore;
	
	static void AcquireNextImage()
	{
		acquireSemaphore = ctx.acquireSemaphores[ctx.acquireSemaphoreIndex];
		
		VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX,
			acquireSemaphore, VK_NULL_HANDLE, &ctx.currentImage);
		
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			CreateSwapchain();
			AcquireNextImage();
		}
		else
		{
			CheckRes(result);
			ctx.acquireSemaphoreIndex = (ctx.acquireSemaphoreIndex + 1) % ctx.numSwapchainImages;
		}
	}
	
	void GetDrawableSize(int& width, int& height)
	{
		width = ctx.surfaceExtent.width;
		height = ctx.surfaceExtent.height;
	}
	
	void BeginFrame()
	{
		AcquireNextImage();
		
		//Waits for the frame queue's fence to complete
		VkFence fence = ctx.frameQueueFences[CFrameIdx()];
		CheckRes(vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX));
		CheckRes(vkResetFences(ctx.device, 1, &fence));
		
		ctx.referencedResources[CFrameIdx()].Release();
		
		static const VkCommandBufferBeginInfo beginInfo = 
		{
			/* sType            */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			/* pNext            */ nullptr,
			/* flags            */ VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			/* pInheritanceInfo */ nullptr
		};
		CheckRes(vkBeginCommandBuffer(ctx.immediateCommandBuffers[CFrameIdx()], &beginInfo));
		
		ctx.defaultFramebufferInPresentMode = true;
		ctx.immediateCCState.pipeline = nullptr;
		ctx.immediateCCState.scissorOutOfDate = true;
		ctx.immediateCCState.viewportOutOfDate = true;
	}
	
	void EndFrame()
	{
		VkCommandBuffer immediateCB = ctx.immediateCommandBuffers[CFrameIdx()];
		
		if (!ctx.defaultFramebufferInPresentMode)
		{
			VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			barrier.image = ctx.swapchainImages[ctx.currentImage];
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			
			vkCmdPipelineBarrier(immediateCB, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier);
		}
		else
		{
			VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			barrier.image = ctx.swapchainImages[ctx.currentImage];
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			
			vkCmdPipelineBarrier(immediateCB, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier);
			
			VkClearColorValue clearValue = { };
			VkImageSubresourceRange clearRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdClearColorImage(immediateCB, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &clearRange);
			
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = 0;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			vkCmdPipelineBarrier(immediateCB, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier);
		}
		
		CheckRes(vkEndCommandBuffer(immediateCB));
		
		const VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		
		const VkSubmitInfo submitInfo =
		{
			/* sType                */ VK_STRUCTURE_TYPE_SUBMIT_INFO,
			/* pNext                */ nullptr,
			/* waitSemaphoreCount   */ 1,
			/* pWaitSemaphores      */ &acquireSemaphore,
			/* pWaitDstStageMask    */ &waitStages,
			/* commandBufferCount   */ 1,
			/* pCommandBuffers      */ &immediateCB,
			/* signalSemaphoreCount */ 1,
			/* pSignalSemaphores    */ &ctx.frameQueueSemaphores[CFrameIdx()]
		};
		
		VkResult presentResult;
		const VkPresentInfoKHR presentInfo =
		{
			/* sType              */ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			/* pNext              */ nullptr,
			/* waitSemaphoreCount */ 1,
			/* pWaitSemaphores    */ &ctx.frameQueueSemaphores[CFrameIdx()],
			/* swapchainCount     */ 1,
			/* pSwapchains        */ &ctx.swapchain,
			/* pImageIndices      */ &ctx.currentImage,
			/* pResults           */ &presentResult
		};
		
		CheckRes(vkQueueSubmit(ctx.mainQueue, 1, &submitInfo, ctx.frameQueueFences[CFrameIdx()]));
		
		CheckRes(vkQueuePresentKHR(ctx.mainQueue, &presentInfo));
		
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
		{
			CreateSwapchain();
		}
		else
		{
			CheckRes(presentResult);
		}
	}
}

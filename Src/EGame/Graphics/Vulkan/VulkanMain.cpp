#ifndef EG_NO_VULKAN
#include "VulkanMain.hpp"
#include "Common.hpp"
#include "Sampler.hpp"
#include "Buffer.hpp"
#include "Translation.hpp"
#include "RenderPasses.hpp"
#include "../RenderDoc.hpp"
#include "../../Core.hpp"

#include <SDL_vulkan.h>
#include <volk.h>
#include <sstream>
#include <cstring>

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
		
		if (CanUsePresentMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR))
		{
			Log(LogLevel::Info, "vk", "Selected present mode: fifo_relaxed");
			return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
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
	
	static SDL_Window* vulkanWindow;
	
	static void CreateSwapchain()
	{
		vkQueueWaitIdle(ctx.mainQueue);
		
		VkSurfaceCapabilitiesKHR capabilities;
		CheckRes(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physDevice, ctx.surface, &capabilities));
		ctx.surfaceExtent = capabilities.currentExtent;
		
		if (ctx.surfaceExtent.width == 0xFFFFFFFF)
		{
			SDL_GetWindowSize(vulkanWindow, reinterpret_cast<int*>(&ctx.surfaceExtent.width),
			                  reinterpret_cast<int*>(&ctx.surfaceExtent.height));
		}
		
		VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		swapchainCreateInfo.surface = ctx.surface;
		swapchainCreateInfo.minImageCount = std::max<uint32_t>(capabilities.minImageCount, 3);
		swapchainCreateInfo.imageFormat = ctx.surfaceFormat.format;
		swapchainCreateInfo.imageColorSpace = ctx.surfaceFormat.colorSpace;
		swapchainCreateInfo.imageExtent = ctx.surfaceExtent;
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
		if (numSwapchainImages > 16)
		{
			EG_PANIC("Too many swapchain images!");
		}
		vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &numSwapchainImages, ctx.swapchainImages);
		
		//Destroys old swapchain image views
		for (uint32_t i = 0; i < numSwapchainImages; i++)
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
		
		//Creates new image semaphores if the number of images has increased
		for (uint32_t i = ctx.numSwapchainImages; i < numSwapchainImages; i++)
		{
			ctx.acquireSemaphores[i] = CreateSemaphore(ctx.device);
		}
		
		DestroyDefaultFramebuffer();
		
		RenderPassDescription defaultFBRenderPassDesc;
		defaultFBRenderPassDesc.numColorAttachments = 1;
		defaultFBRenderPassDesc.numResolveColorAttachments = 0;
		defaultFBRenderPassDesc.colorAttachments[0].format = ctx.surfaceFormat.format;
		defaultFBRenderPassDesc.colorAttachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
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
			defaultFBRenderPassDesc.depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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
	
	static const char* REQUIRED_DEVICE_EXTENSIONS[] = 
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,
		VK_KHR_MAINTENANCE2_EXTENSION_NAME,
		VK_KHR_MULTIVIEW_EXTENSION_NAME,
		VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME
	};
	
	static const char* OPTIONAL_DEVICE_EXTENSIONS[] = 
	{
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
		VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
		VK_KHR_BIND_MEMORY_2_EXTENSION_NAME
	};
	
	static inline std::string_view GetVendorName(uint32_t id)
	{
		switch (id)
		{
		case 0x1002: return "AMD";
		case 0x1010: return "ImgTec";
		case 0x10DE: return "Nvidia";
		case 0x13B5: return "ARM";
		case 0x5143: return "Qualcomm";
		case 0x8086: return "Intel";
		default:     return "Unknown";
		}
	}
	
	static std::vector<const char*> instanceExtensionsToEnable;
	static std::vector<VkExtensionProperties> instanceExtensionProperties;
	
	static inline bool InstanceExtensionSupported(const char* name)
	{
		for (const VkExtensionProperties& extension : instanceExtensionProperties)
		{
			if (std::strcmp(extension.extensionName, name) == 0)
				return true;
		}
		return false;
	}
	
	static std::optional<bool> earlyInitializeResult;
	
	bool EarlyInitializeMemoized()
	{
		if (earlyInitializeResult.has_value())
			return earlyInitializeResult.value();
		
		if (volkInitialize() != VK_SUCCESS || SDL_Vulkan_LoadLibrary(nullptr) != 0)
		{
			earlyInitializeResult = false;
			return false;
		}
		
		//Enumerates supported instance extensions
		uint32_t availInstanceExtensions;
		vkEnumerateInstanceExtensionProperties(nullptr, &availInstanceExtensions, nullptr);
		instanceExtensionProperties.resize(availInstanceExtensions);
		vkEnumerateInstanceExtensionProperties(nullptr, &availInstanceExtensions, instanceExtensionProperties.data());
		
		//Selects instance extensions
		uint32_t sdlNumInstanceExtensions;
		SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlNumInstanceExtensions, nullptr);
		instanceExtensionsToEnable.resize(sdlNumInstanceExtensions);
		SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlNumInstanceExtensions, instanceExtensionsToEnable.data());
		
		instanceExtensionsToEnable.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		
		for (const char* extensionName : instanceExtensionsToEnable)
		{
			if (!InstanceExtensionSupported(extensionName))
			{
				earlyInitializeResult = false;
				return false;
			}
		}
		
		earlyInitializeResult = true;
		return true;
	}
	
	static std::vector<std::string> okDeviceNames;
	
	std::span<std::string> GetDeviceNames()
	{
		return okDeviceNames;
	}
	
	bool Initialize(const GraphicsAPIInitArguments& initArguments)
	{
		if (!EarlyInitializeMemoized())
			return false;
		
		//Enumerates instance layers
		uint32_t availableInstanceLayers;
		vkEnumerateInstanceLayerProperties(&availableInstanceLayers, nullptr);
		std::vector<VkLayerProperties> layerProperties(availableInstanceLayers);
		vkEnumerateInstanceLayerProperties(&availableInstanceLayers, layerProperties.data());
		auto IsLayerSupported = [&] (const char* name)
		{
			for (VkLayerProperties& layer : layerProperties)
			{
				if (std::strcmp(layer.layerName, name) == 0)
					return true;
			}
			return false;
		};
		
		std::vector<const char*> enabledValidationLayers;
		auto MaybeEnableValidationLayer = [&] (const char* name)
		{
			if (IsLayerSupported(name))
			{
				enabledValidationLayers.push_back(name);
				return true;
			}
			return false;
		};
		
		ctx.hasDebugUtils = false;
		if (DevMode() && InstanceExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
		{
			ctx.hasDebugUtils = true;
			instanceExtensionsToEnable.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			
			if (!MaybeEnableValidationLayer("VK_LAYER_KHRONOS_validation") &&
				!MaybeEnableValidationLayer("VK_LAYER_LUNARG_standard_validation"))
			{
				eg::Log(eg::LogLevel::Warning, "vk", "Could not enable validation layers, no supported layer found.");
			}
		}
		
		VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		applicationInfo.pApplicationName = GameName().c_str();
		applicationInfo.pEngineName = "EGame";
		applicationInfo.apiVersion = VK_API_VERSION_1_0;
		
		//Creates the instance
		const VkInstanceCreateInfo instanceCreateInfo =
		{
			/* sType                   */ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			/* pNext                   */ nullptr,
			/* flags                   */ 0,
			/* pApplicationInfo        */ &applicationInfo,
			/* enabledLayerCount       */ (uint32_t)enabledValidationLayers.size(),
			/* ppEnabledLayerNames     */ enabledValidationLayers.data(),
			/* enabledExtensionCount   */ (uint32_t)instanceExtensionsToEnable.size(),
			/* ppEnabledExtensionNames */ instanceExtensionsToEnable.data()
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
		
		auto GetDevicePreferenceIndex = [&] (const VkPhysicalDeviceProperties& properties) -> int
		{
			if (std::string_view(properties.deviceName) == initArguments.preferredDeviceName)
				return -1;
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				return initArguments.preferIntegrated ? 1 : 0;
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
				return initArguments.preferIntegrated ? 0 : 1;
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
				return 2;
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
				return 3;
			return 4;
		};
		
		auto IsDevicePreferredOver = [&] (const VkPhysicalDeviceProperties& candidate, const VkPhysicalDeviceProperties& current)
		{
			return GetDevicePreferenceIndex(candidate) < GetDevicePreferenceIndex(current);
		};
		
		//Selects which physical device to use
		bool optionalExtensionsSeen[ArrayLen(OPTIONAL_DEVICE_EXTENSIONS)];
		VkPhysicalDeviceProperties currentDeviceProperties;
		okDeviceNames.clear();
		for (VkPhysicalDevice physicalDevice : physicalDevices)
		{
			if (physicalDevice == nullptr)
				continue;
			
			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
			
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
			
			//Enumerates the queue families exposed by this device
			uint32_t numQueueFamilies;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, queueFamilyProperties.data());
			
			//Searches for a compatible queue family
			const int REQUIRED_QUEUE_FLAGS = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
			std::optional<uint32_t> selectedQueueFamily;
			VkQueueFamilyProperties selectedQueueFamilyProperties;
			for (uint32_t i = 0; i < numQueueFamilies; i++)
			{
				VkBool32 surfaceSupported;
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, ctx.surface, &surfaceSupported);
				if (!surfaceSupported)
					continue;
				
				if ((queueFamilyProperties[i].queueFlags & REQUIRED_QUEUE_FLAGS) == REQUIRED_QUEUE_FLAGS)
				{
					selectedQueueFamily = i;
					selectedQueueFamilyProperties = queueFamilyProperties[i];
					break;
				}
			}
			
			if (!selectedQueueFamily.has_value())
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
			std::fill(std::begin(optionalExtensionsSeen), std::end(optionalExtensionsSeen), false);
			bool requiredExtensionsSeen[ArrayLen(REQUIRED_DEVICE_EXTENSIONS)] = { };
			for (const VkExtensionProperties& extProperties : devExtensionProperties)
			{
				for (size_t i = 0; i < ArrayLen(REQUIRED_DEVICE_EXTENSIONS); i++)
				{
					if (std::strcmp(REQUIRED_DEVICE_EXTENSIONS[i], extProperties.extensionName) == 0)
					{
						requiredExtensionsSeen[i] = true;
						break;
					}
				}
				for (size_t i = 0; i < ArrayLen(OPTIONAL_DEVICE_EXTENSIONS); i++)
				{
					if (std::strcmp(OPTIONAL_DEVICE_EXTENSIONS[i], extProperties.extensionName) == 0)
					{
						optionalExtensionsSeen[i] = true;
						break;
					}
				}
			}
			
			bool hasAllExtensions = true;
			for (size_t i = 0; i < ArrayLen(REQUIRED_DEVICE_EXTENSIONS); i++)
			{
				if (!requiredExtensionsSeen[i])
				{
					Log(LogLevel::Info, "vk", "Cannot use vulkan device '{0}' because it does not support the "
						"{1} extension", deviceProperties.deviceName, REQUIRED_DEVICE_EXTENSIONS[i]);
					hasAllExtensions = false;
					break;
				}
			}
			
			if (!hasAllExtensions)
				continue;
			
			okDeviceNames.emplace_back(deviceProperties.deviceName);
			
			if (ctx.physDevice != VK_NULL_HANDLE && !IsDevicePreferredOver(deviceProperties, currentDeviceProperties))
				continue;
			
			ctx.queueFamily = *selectedQueueFamily;
			ctx.queueFamilyProperties = selectedQueueFamilyProperties;
			ctx.physDevice = physicalDevice;
			ctx.deviceFeatures = deviceFeatures;
			ctx.deviceName = deviceProperties.deviceName;
			ctx.deviceVendorName = GetVendorName(deviceProperties.vendorID);
			ctx.deviceLimits = deviceProperties.limits;
			currentDeviceProperties = deviceProperties;
		}
		
		if (ctx.physDevice == VK_NULL_HANDLE)
		{
			Log(LogLevel::Error, "vk", "No compatible vulkan device was found");
			return false;
		}
		
		vulkanWindow = initArguments.window;
		
		if (okDeviceNames.size() > 1)
		{
			std::ostringstream namesConcatStream;
			namesConcatStream << "'" << okDeviceNames[0] << "'";
			for (size_t i = 1; i < okDeviceNames.size(); i++)
				namesConcatStream << ", '" << okDeviceNames[i] << "'";
			Log(LogLevel::Info, "vk", "Multiple usable vulkan devices: {0}", namesConcatStream.str());
		}
		
		Log(LogLevel::Info, "vk", "Using vulkan device: '{0}'", ctx.deviceName);
		
		vkGetPhysicalDeviceMemoryProperties(ctx.physDevice, &ctx.memoryProperties);
		
		bool supportsMultipleGraphicsQueues = ctx.queueFamilyProperties.queueCount > 1;
		
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
		enabledDeviceFeatures.shaderStorageImageExtendedFormats = ctx.deviceFeatures.shaderStorageImageExtendedFormats;
		enabledDeviceFeatures.imageCubeArray = ctx.deviceFeatures.imageCubeArray;
		enabledDeviceFeatures.samplerAnisotropy = ctx.deviceFeatures.samplerAnisotropy;
		enabledDeviceFeatures.independentBlend = ctx.deviceFeatures.independentBlend;
		enabledDeviceFeatures.fillModeNonSolid = ctx.deviceFeatures.fillModeNonSolid;
		enabledDeviceFeatures.geometryShader = ctx.deviceFeatures.geometryShader;
		enabledDeviceFeatures.tessellationShader = ctx.deviceFeatures.tessellationShader;
		enabledDeviceFeatures.depthClamp = ctx.deviceFeatures.depthClamp;
		enabledDeviceFeatures.shaderClipDistance = ctx.deviceFeatures.shaderClipDistance;
		enabledDeviceFeatures.shaderCullDistance = ctx.deviceFeatures.shaderCullDistance;
		enabledDeviceFeatures.textureCompressionBC = ctx.deviceFeatures.textureCompressionBC;
		enabledDeviceFeatures.fragmentStoresAndAtomics = ctx.deviceFeatures.fragmentStoresAndAtomics;
		enabledDeviceFeatures.wideLines = ctx.deviceFeatures.wideLines;
		
		uint32_t numEnabledDeviceExtensions = ArrayLen(REQUIRED_DEVICE_EXTENSIONS);
		const char* enabledDeviceExtensions[32];
		std::copy(std::begin(REQUIRED_DEVICE_EXTENSIONS), std::end(REQUIRED_DEVICE_EXTENSIONS), enabledDeviceExtensions);
		
		//Enables optional device extensions
		for (size_t i = 0; i < ArrayLen(OPTIONAL_DEVICE_EXTENSIONS); i++)
		{
			if (optionalExtensionsSeen[i])
			{
				enabledDeviceExtensions[numEnabledDeviceExtensions++] = OPTIONAL_DEVICE_EXTENSIONS[i];
			}
		}
		
		auto OptionalExtensionAvailable = [&] (const char* name)
		{
			for (size_t i = 0; i < ArrayLen(OPTIONAL_DEVICE_EXTENSIONS); i++)
			{
				if (std::strcmp(OPTIONAL_DEVICE_EXTENSIONS[i], name) == 0 && optionalExtensionsSeen[i])
					return true;
			}
			return false;
		};
		
		const bool hasDedicatedAllocation =
			OptionalExtensionAvailable(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) &&
			OptionalExtensionAvailable(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) && !renderdoc::IsPresent();
		const bool hasBindMemory2 = OptionalExtensionAvailable(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
		
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
		allocatorVulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
		allocatorVulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
		allocatorVulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
		allocatorVulkanFunctions.vkBindImageMemory = vkBindImageMemory;
		allocatorVulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
		allocatorVulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
		allocatorVulkanFunctions.vkCreateBuffer = vkCreateBuffer;
		allocatorVulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
		allocatorVulkanFunctions.vkCreateImage = vkCreateImage;
		allocatorVulkanFunctions.vkDestroyImage = vkDestroyImage;
		allocatorVulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
		
		VmaAllocatorCreateInfo allocatorCreateInfo = { };
		allocatorCreateInfo.physicalDevice = ctx.physDevice;
		allocatorCreateInfo.device = ctx.device;
		allocatorCreateInfo.instance = ctx.instance;
		allocatorCreateInfo.pVulkanFunctions = &allocatorVulkanFunctions;
		
		if (hasDedicatedAllocation)
		{
			allocatorVulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
			allocatorVulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
			allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		}
		if (hasBindMemory2)
		{
			allocatorVulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
			allocatorVulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
			allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
		}
		CheckRes(vmaCreateAllocator(&allocatorCreateInfo, &ctx.allocator));
		
		ctx.surfaceFormat = SelectSurfaceFormat(initArguments.defaultFramebufferSRGB);
		if (ctx.surfaceFormat.format == VK_FORMAT_UNDEFINED)
			return false;
		
		ctx.presentMode = SelectPresentMode(true);
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
	
	void GetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
	{
		deviceInfo.uniformBufferOffsetAlignment   = (uint32_t)ctx.deviceLimits.minUniformBufferOffsetAlignment;
		deviceInfo.storageBufferOffsetAlignment   = (uint32_t)ctx.deviceLimits.minStorageBufferOffsetAlignment;
		deviceInfo.depthRange                     = DepthRange::ZeroToOne;
		deviceInfo.tessellation                   = ctx.deviceFeatures.tessellationShader;
		deviceInfo.geometryShader                 = ctx.deviceFeatures.geometryShader;
		deviceInfo.maxTessellationPatchSize       = ctx.deviceLimits.maxTessellationPatchSize;
		deviceInfo.maxClipDistances               = ctx.deviceFeatures.shaderClipDistance ? ctx.deviceLimits.maxClipDistances : 0;
		deviceInfo.maxMSAA                        = ctx.deviceLimits.sampledImageColorSampleCounts;
		deviceInfo.computeShader                  = true;
		deviceInfo.textureCubeMapArray            = ctx.deviceFeatures.imageCubeArray;
		deviceInfo.blockTextureCompression        = ctx.deviceFeatures.textureCompressionBC;
		deviceInfo.timerTicksPerNS                = ctx.deviceLimits.timestampPeriod;
		deviceInfo.concurrentResourceCreation     = true;
		deviceInfo.maxComputeWorkGroupInvocations = ctx.deviceLimits.maxComputeWorkGroupInvocations;
		deviceInfo.maxComputeWorkGroupInvocations = ctx.deviceLimits.maxComputeWorkGroupInvocations;
		deviceInfo.deviceName                     = ctx.deviceName;
		deviceInfo.deviceVendorName               = ctx.deviceVendorName;
		std::copy_n(ctx.deviceLimits.maxComputeWorkGroupCount, 3, deviceInfo.maxComputeWorkGroupCount);
		std::copy_n(ctx.deviceLimits.maxComputeWorkGroupSize, 3, deviceInfo.maxComputeWorkGroupSize);
	}
	
	void SetEnableVSync(bool enableVSync)
	{
		ctx.presentMode = SelectPresentMode(enableVSync);
		CreateSwapchain();
	}
	
	GraphicsMemoryStat GetMemoryStat()
	{
		VmaStats vmaStats;
		vmaCalculateStats(ctx.allocator, &vmaStats);
		GraphicsMemoryStat stat;
		stat.allocatedBytes = vmaStats.total.usedBytes;
		stat.numBlocks = vmaStats.total.blockCount;
		stat.unusedRanges = vmaStats.total.unusedRangeCount;
		
		stat.allocatedBytesGPU = 0;
		for (uint32_t h = 0; h < ctx.memoryProperties.memoryHeapCount; h++)
		{
			if (ctx.memoryProperties.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			{
				stat.allocatedBytesGPU += vmaStats.memoryHeap[h].usedBytes;
			}
		}
		
		return stat;
	}
	
	void DestroyCachedDescriptorSets();
	
	void Shutdown()
	{
		vkDeviceWaitIdle(ctx.device);
		
		ProcessPendingInitBuffers(true);
		
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
	
	void DeviceWaitIdle()
	{
		CheckRes(vkDeviceWaitIdle(ctx.device));
	}
	
	static VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
	
	void MaybeAcquireSwapchainImage()
	{
		if (acquireSemaphore != VK_NULL_HANDLE)
			return;
		
		VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX,
			ctx.acquireSemaphores[ctx.acquireSemaphoreIndex], VK_NULL_HANDLE, &ctx.currentImage);
		
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			CreateSwapchain();
			MaybeAcquireSwapchainImage();
		}
		else
		{
			CheckRes(result);
			acquireSemaphore = ctx.acquireSemaphores[ctx.acquireSemaphoreIndex];
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
		acquireSemaphore = VK_NULL_HANDLE;
		
		//Waits for the frame queue's fence to complete
		VkFence fence = ctx.frameQueueFences[CFrameIdx()];
		CheckRes(vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX));
		CheckRes(vkResetFences(ctx.device, 1, &fence));
		
		ProcessPendingInitBuffers(false);
		
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
		MaybeAcquireSwapchainImage();
		
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
		
		vkQueuePresentKHR(ctx.mainQueue, &presentInfo);
		
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
		{
			CreateSwapchain();
		}
		else
		{
			CheckRes(presentResult);
		}
	}
	
	static inline void InitLabelInfo(VkDebugUtilsLabelEXT& labelInfo, const char* label, const float* color)
	{
		labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		labelInfo.pNext = nullptr;
		labelInfo.pLabelName = label;
		if (color != nullptr)
		{
			std::copy_n(color, 4, labelInfo.color);
		}
		else
		{
			std::fill_n(labelInfo.color, 4, 0.0f);
		}
	}
	
	void DebugLabelBegin(CommandContextHandle cctx, const char* label, const float* color)
	{
		if (vkCmdBeginDebugUtilsLabelEXT != nullptr)
		{
			VkDebugUtilsLabelEXT labelInfo;
			InitLabelInfo(labelInfo, label, color);
			vkCmdBeginDebugUtilsLabelEXT(GetCB(cctx), &labelInfo);
		}
	}
	
	void DebugLabelEnd(CommandContextHandle cctx)
	{
		if (vkCmdEndDebugUtilsLabelEXT != nullptr)
		{
			vkCmdEndDebugUtilsLabelEXT(GetCB(cctx));
		}
	}
	
	void DebugLabelInsert(CommandContextHandle cctx, const char* label, const float* color)
	{
		if (vkCmdInsertDebugUtilsLabelEXT != nullptr)
		{
			VkDebugUtilsLabelEXT labelInfo;
			InitLabelInfo(labelInfo, label, color);
			vkCmdInsertDebugUtilsLabelEXT(GetCB(cctx), &labelInfo);
		}
	}
}

#endif

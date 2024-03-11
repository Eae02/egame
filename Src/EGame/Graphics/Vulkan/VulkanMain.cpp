#include "EGame/Graphics/Abstraction.hpp"
#include <vulkan/vulkan_core.h>
#ifndef EG_NO_VULKAN
#include "../../Assert.hpp"
#include "../../Core.hpp"
#include "../RenderDoc.hpp"
#include "Buffer.hpp"
#include "CachedDescriptorSetLayout.hpp"
#include "Common.hpp"
#include "RenderPasses.hpp"
#include "Translation.hpp"
#include "VulkanMain.hpp"

#include "VulkanCommandContext.hpp"
#include <SDL_vulkan.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <volk.h>

namespace eg::graphics_api::vk
{
Context ctx;

static const char* REQUIRED_DEVICE_EXTENSIONS[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
	VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
};

static const char* OPTIONAL_DEVICE_EXTENSIONS[] = {
	VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
	VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
	VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,
	VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME,
};

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

static constexpr uint32_t VULKAN_API_VERSION = VK_API_VERSION_1_1;

bool EarlyInitializeMemoized()
{
	if (earlyInitializeResult.has_value())
		return earlyInitializeResult.value();

	if (volkInitialize() != VK_SUCCESS || SDL_Vulkan_LoadLibrary(nullptr) != 0)
	{
		earlyInitializeResult = false;
		return false;
	}

	uint32_t vulkanApiVersion = 0;
	vkEnumerateInstanceVersion(&vulkanApiVersion);
	if (vulkanApiVersion < VULKAN_API_VERSION)
	{
		earlyInitializeResult = false;
		return false;
	}

	// Enumerates supported instance extensions
	uint32_t availInstanceExtensions;
	vkEnumerateInstanceExtensionProperties(nullptr, &availInstanceExtensions, nullptr);
	instanceExtensionProperties.resize(availInstanceExtensions);
	vkEnumerateInstanceExtensionProperties(nullptr, &availInstanceExtensions, instanceExtensionProperties.data());

	// Selects instance extensions
	uint32_t sdlNumInstanceExtensions;
	SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlNumInstanceExtensions, nullptr);
	instanceExtensionsToEnable.resize(sdlNumInstanceExtensions);
	SDL_Vulkan_GetInstanceExtensions(nullptr, &sdlNumInstanceExtensions, instanceExtensionsToEnable.data());

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

	// Enumerates instance layers
	uint32_t availableInstanceLayers;
	vkEnumerateInstanceLayerProperties(&availableInstanceLayers, nullptr);
	std::vector<VkLayerProperties> layerProperties(availableInstanceLayers);
	vkEnumerateInstanceLayerProperties(&availableInstanceLayers, layerProperties.data());
	auto IsLayerSupported = [&](const char* name)
	{
		for (VkLayerProperties& layer : layerProperties)
		{
			if (std::strcmp(layer.layerName, name) == 0)
				return true;
		}
		return false;
	};

	std::vector<const char*> enabledValidationLayers;
	auto MaybeEnableValidationLayer = [&](const char* name)
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
	applicationInfo.apiVersion = VULKAN_API_VERSION;

	// Creates the instance
	const VkInstanceCreateInfo instanceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &applicationInfo,
		.enabledLayerCount = UnsignedNarrow<uint32_t>(enabledValidationLayers.size()),
		.ppEnabledLayerNames = enabledValidationLayers.data(),
		.enabledExtensionCount = UnsignedNarrow<uint32_t>(instanceExtensionsToEnable.size()),
		.ppEnabledExtensionNames = instanceExtensionsToEnable.data(),
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

	// Enumerates physical devices
	uint32_t numDevices;
	vkEnumeratePhysicalDevices(ctx.instance, &numDevices, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(numDevices);
	vkEnumeratePhysicalDevices(ctx.instance, &numDevices, physicalDevices.data());

	auto GetDevicePreferenceIndex = [&](const VkPhysicalDeviceProperties& properties) -> int
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

	auto IsDevicePreferredOver =
		[&](const VkPhysicalDeviceProperties& candidate, const VkPhysicalDeviceProperties& current)
	{ return GetDevicePreferenceIndex(candidate) < GetDevicePreferenceIndex(current); };

	// Selects which physical device to use
	std::array<bool, std::size(OPTIONAL_DEVICE_EXTENSIONS)> optionalExtensionsSeen;
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

		// Enumerates the queue families exposed by this device
		uint32_t numQueueFamilies;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, queueFamilyProperties.data());

		// Searches for a compatible queue family
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
			Log(LogLevel::Info, "vk",
			    "Cannot use vulkan device '{0}' because it does not have a queue family "
			    " that supports graphics, compute and present.",
			    deviceProperties.deviceName);
			continue;
		}

		// Enumerates supported device extensions
		uint32_t availDevExtensions;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availDevExtensions, nullptr);
		std::vector<VkExtensionProperties> devExtensionProperties(availDevExtensions);
		vkEnumerateDeviceExtensionProperties(
			physicalDevice, nullptr, &availDevExtensions, devExtensionProperties.data());

		// Checks which device extensions are supported
		std::array<bool, std::size(OPTIONAL_DEVICE_EXTENSIONS)> optionalExtensionsSeenThisDevice;
		std::array<bool, std::size(REQUIRED_DEVICE_EXTENSIONS)> requiredExtensionsSeen;
		for (const VkExtensionProperties& extProperties : devExtensionProperties)
		{
			for (size_t i = 0; i < std::size(REQUIRED_DEVICE_EXTENSIONS); i++)
			{
				if (std::strcmp(REQUIRED_DEVICE_EXTENSIONS[i], extProperties.extensionName) == 0)
				{
					requiredExtensionsSeen[i] = true;
					break;
				}
			}
			for (size_t i = 0; i < std::size(OPTIONAL_DEVICE_EXTENSIONS); i++)
			{
				if (std::strcmp(OPTIONAL_DEVICE_EXTENSIONS[i], extProperties.extensionName) == 0)
				{
					optionalExtensionsSeenThisDevice[i] = true;
					break;
				}
			}
		}

		bool hasAllExtensions = true;
		for (size_t i = 0; i < std::size(REQUIRED_DEVICE_EXTENSIONS); i++)
		{
			if (!requiredExtensionsSeen[i])
			{
				Log(LogLevel::Info, "vk",
				    "Cannot use vulkan device '{0}' because it does not support the "
				    "{1} extension",
				    deviceProperties.deviceName, REQUIRED_DEVICE_EXTENSIONS[i]);
				hasAllExtensions = false;
				break;
			}
		}

		if (!hasAllExtensions)
			continue;

		okDeviceNames.emplace_back(deviceProperties.deviceName);

		if (ctx.physDevice != VK_NULL_HANDLE && !IsDevicePreferredOver(deviceProperties, currentDeviceProperties))
		{
			continue;
		}

		ctx.queueFamily = *selectedQueueFamily;
		ctx.queueFamilyProperties = selectedQueueFamilyProperties;
		ctx.physDevice = physicalDevice;
		ctx.deviceFeatures = deviceFeatures;
		ctx.deviceName = deviceProperties.deviceName;
		ctx.deviceLimits = deviceProperties.limits;
		optionalExtensionsSeen = optionalExtensionsSeenThisDevice;
		currentDeviceProperties = deviceProperties;
	}

	if (ctx.physDevice == VK_NULL_HANDLE)
	{
		Log(LogLevel::Error, "vk", "No compatible vulkan device was found");
		return false;
	}

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

	// Creates the logical device
	const float queuePriorities[] = { 1 };
	const VkDeviceQueueCreateInfo queueCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = ctx.queueFamily,
		.queueCount = 1,
		.pQueuePriorities = queuePriorities,
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

	std::vector<const char*> enabledDeviceExtensions(
		std::begin(REQUIRED_DEVICE_EXTENSIONS), std::end(REQUIRED_DEVICE_EXTENSIONS));

	// Enables optional device extensions
	for (size_t i = 0; i < std::size(OPTIONAL_DEVICE_EXTENSIONS); i++)
	{
		if (optionalExtensionsSeen[i])
		{
			enabledDeviceExtensions.push_back(OPTIONAL_DEVICE_EXTENSIONS[i]);
		}
	}

	auto OptionalExtensionAvailable = [&](const char* name)
	{
		for (size_t i = 0; i < std::size(OPTIONAL_DEVICE_EXTENSIONS); i++)
		{
			if (std::strcmp(OPTIONAL_DEVICE_EXTENSIONS[i], name) == 0 && optionalExtensionsSeen[i])
				return true;
		}
		return false;
	};

	VkDeviceCreateInfo deviceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueCreateInfo,
		.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size()),
		.ppEnabledExtensionNames = enabledDeviceExtensions.data(),
		.pEnabledFeatures = &enabledDeviceFeatures,
	};

	VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateEnabledFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
		.extendedDynamicState = VK_TRUE,
	};
	PushPNext(deviceCreateInfo, extendedDynamicStateEnabledFeatures);

	// ** Queries physical device features using VkPhysicalDeviceFeatures2

	VkPhysicalDeviceFeatures2 physDeviceFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

	// Checks for support for extended dynamic state 3
	const bool hasDynamicState3Extension = OptionalExtensionAvailable(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
	VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamicState3Features = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
	};
	if (hasDynamicState3Extension)
		PushPNext(physDeviceFeatures2, dynamicState3Features);

	ctx.hasSubgroupSizeControlExtension = OptionalExtensionAvailable(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
	VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT
	};
	if (ctx.hasSubgroupSizeControlExtension)
		PushPNext(physDeviceFeatures2, subgroupSizeControlFeatures);

	VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pipelineExecutablePropertiesFeatures = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR
	};
	if (OptionalExtensionAvailable(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME))
		PushPNext(physDeviceFeatures2, pipelineExecutablePropertiesFeatures);

	vkGetPhysicalDeviceFeatures2(ctx.physDevice, &physDeviceFeatures2);

	// ** Queries physical device properties using VkPhysicalDeviceProperties2
	VkPhysicalDeviceProperties2 deviceProperties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };

	VkPhysicalDeviceSubgroupProperties subgroupProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
	PushPNext(deviceProperties2, subgroupProperties);

	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT
	};
	if (ctx.hasSubgroupSizeControlExtension)
		PushPNext(deviceProperties2, subgroupSizeControlProperties);

	vkGetPhysicalDeviceProperties2(ctx.physDevice, &deviceProperties2);

	// Sets up ctx.subgroupFeatures
	const auto subgroupFeatureFlags = static_cast<SubgroupFeatureFlags>(subgroupProperties.supportedOperations);
	if (ctx.hasSubgroupSizeControlExtension)
	{
		ctx.subgroupFeatures = SubgroupFeatures{
			.minSubgroupSize = subgroupSizeControlProperties.minSubgroupSize,
			.maxSubgroupSize = subgroupSizeControlProperties.maxSubgroupSize,
			.maxWorkgroupSubgroups = subgroupSizeControlProperties.maxComputeWorkgroupSubgroups,
			.supportsRequireFullSubgroups = subgroupSizeControlFeatures.computeFullSubgroups == VK_TRUE,
			.supportsRequiredSubgroupSize =
				(subgroupSizeControlProperties.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0,
			.supportsGetPipelineSubgroupSize = pipelineExecutablePropertiesFeatures.pipelineExecutableInfo == VK_TRUE,
			.featureFlags = subgroupFeatureFlags,
		};
	}

	// Enables support for dynamic state from extended dynamic state 3
	VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extendedDynamicState3EnabledFeatures = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
	};
	if (hasDynamicState3Extension)
	{
		if (dynamicState3Features.extendedDynamicState3PolygonMode)
		{
			extendedDynamicState3EnabledFeatures.extendedDynamicState3PolygonMode = true;
			ctx.hasDynamicStatePolygonMode = true;
		}
		PushPNext(deviceCreateInfo, extendedDynamicState3EnabledFeatures);
	}

	VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlEnabledFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT,
		.subgroupSizeControl = subgroupSizeControlFeatures.subgroupSizeControl,
		.computeFullSubgroups = subgroupSizeControlFeatures.computeFullSubgroups,
	};
	if (ctx.hasSubgroupSizeControlExtension)
		PushPNext(deviceCreateInfo, subgroupSizeControlEnabledFeatures);

	VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pipelineExecutablePropertiesEnabledFeatures = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR,
		.pipelineExecutableInfo = pipelineExecutablePropertiesFeatures.pipelineExecutableInfo,
	};
	if (OptionalExtensionAvailable(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME))
		PushPNext(deviceCreateInfo, pipelineExecutablePropertiesEnabledFeatures);

	ctx.hasPushDescriptorExtension = OptionalExtensionAvailable(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

	// ** Creates the device **

	VkResult createDeviceRes = vkCreateDevice(ctx.physDevice, &deviceCreateInfo, nullptr, &ctx.device);
	if (createDeviceRes != VK_SUCCESS)
	{
		Log(LogLevel::Error, "vk", "Vulkan device creation failed with status: {0}", createDeviceRes);
		return false;
	}

	volkLoadDevice(ctx.device);

	// Gets queue handles
	vkGetDeviceQueue(ctx.device, ctx.queueFamily, 0, &ctx.mainQueue);

	// Creates the main command pool
	const VkCommandPoolCreateInfo mainCommandPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = ctx.queueFamily,
	};
	CheckRes(vkCreateCommandPool(ctx.device, &mainCommandPoolCreateInfo, nullptr, &ctx.mainCommandPool));

	// Creates the debug messenger
	if (ctx.hasDebugUtils)
	{
		const VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
			.pfnUserCallback = DebugCallback,
		};
		CheckRes(vkCreateDebugUtilsMessengerEXT(ctx.instance, &messengerCreateInfo, nullptr, &ctx.debugMessenger));
	}

	const VmaVulkanFunctions allocatorVulkanFunctions = {
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
		.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
		.vkBindBufferMemory2KHR = vkBindBufferMemory2,
		.vkBindImageMemory2KHR = vkBindImageMemory2,
	};

	const VmaAllocatorCreateInfo allocatorCreateInfo = {
		.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT | VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT,
		.physicalDevice = ctx.physDevice,
		.device = ctx.device,
		.pVulkanFunctions = &allocatorVulkanFunctions,
		.instance = ctx.instance,
		.vulkanApiVersion = VULKAN_API_VERSION,
	};

	CheckRes(vmaCreateAllocator(&allocatorCreateInfo, &ctx.allocator));

	if (!ctx.swapchain.Init(initArguments))
		return false;

	ctx.defaultDSFormat = TranslateFormat(initArguments.defaultDepthStencilFormat);
	ctx.swapchain.Create();

	// Creates frame queue resources
	VkFenceCreateInfo frameQueueFenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
	{
		frameQueueFenceCreateInfo.flags = i == 0 ? 0 : VK_FENCE_CREATE_SIGNALED_BIT;
		CheckRes(vkCreateFence(ctx.device, &frameQueueFenceCreateInfo, nullptr, &ctx.frameQueueFences[i]));
		ctx.frameQueueSemaphores[i] = CreateSemaphore(ctx.device);
	}

	// Allocates immediate command buffers
	const VkCommandBufferAllocateInfo cmdAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = ctx.mainCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = MAX_CONCURRENT_FRAMES,
	};
	VkCommandBuffer immediateCommandBuffers[MAX_CONCURRENT_FRAMES];
	CheckRes(vkAllocateCommandBuffers(ctx.device, &cmdAllocateInfo, immediateCommandBuffers));

	// Initializes immediate contexts
	VulkanCommandContext::immediateContexts.resize(MAX_CONCURRENT_FRAMES);
	for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
	{
		VulkanCommandContext::immediateContexts[i].cb = immediateCommandBuffers[i];
		VulkanCommandContext::immediateContexts[i].SetInitialState();
	}
	VulkanCommandContext::currentImmediate = &VulkanCommandContext::immediateContexts[0];

	// Starts the first immediate command buffer
	const VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(immediateCommandBuffers[0], &beginInfo);

	return true;
}

void GetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
{
	DeviceFeatureFlags features = DeviceFeatureFlags::ComputeShaderAndSSBO |
	                              DeviceFeatureFlags::ConcurrentResourceCreation |
	                              DeviceFeatureFlags::PartialTextureViews | DeviceFeatureFlags::DeferredContext;

	if (ctx.deviceFeatures.tessellationShader)
		features |= DeviceFeatureFlags::TessellationShader;
	if (ctx.deviceFeatures.geometryShader)
		features |= DeviceFeatureFlags::GeometryShader;
	if (ctx.deviceFeatures.imageCubeArray)
		features |= DeviceFeatureFlags::TextureCubeMapArray;
	if (ctx.deviceFeatures.textureCompressionBC)
		features |= DeviceFeatureFlags::TextureCompressionBC;
	if (ctx.deviceFeatures.textureCompressionASTC_LDR)
		features |= DeviceFeatureFlags::TextureCompressionASTC;
	if (ctx.hasPushDescriptorExtension)
		features |= DeviceFeatureFlags::DynamicResourceBind;

	deviceInfo = GraphicsDeviceInfo{
		.uniformBufferOffsetAlignment = UnsignedNarrow<uint32_t>(ctx.deviceLimits.minUniformBufferOffsetAlignment),
		.storageBufferOffsetAlignment = UnsignedNarrow<uint32_t>(ctx.deviceLimits.minStorageBufferOffsetAlignment),
		.maxTessellationPatchSize = ctx.deviceLimits.maxTessellationPatchSize,
		.maxClipDistances = ctx.deviceFeatures.shaderClipDistance ? ctx.deviceLimits.maxClipDistances : 0,
		.maxComputeWorkGroupInvocations = ctx.deviceLimits.maxComputeWorkGroupInvocations,
		.subgroupFeatures = ctx.subgroupFeatures,
		.depthRange = DepthRange::ZeroToOne,
		.features = features,
		.timerTicksPerNS = ctx.deviceLimits.timestampPeriod,
		.deviceName = ctx.deviceName,
		.apiName = "Vulkan",
	};

	std::copy_n(ctx.deviceLimits.maxComputeWorkGroupCount, 3, deviceInfo.maxComputeWorkGroupCount);
	std::copy_n(ctx.deviceLimits.maxComputeWorkGroupSize, 3, deviceInfo.maxComputeWorkGroupSize);
}

static const std::pair<VkFormatFeatureFlags, FormatCapabilities> imageFormatCapabilities[] = {
	{ VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, FormatCapabilities::SampledImage },
	{ VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, FormatCapabilities::SampledImageFilterLinear },
	{ VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT, FormatCapabilities::StorageImage },
	{ VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT, FormatCapabilities::StorageImageAtomic },
	{ VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT, FormatCapabilities::ColorAttachment },
	{ VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT, FormatCapabilities::ColorAttachmentBlend },
	{ VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, FormatCapabilities::DepthStencilAttachment },
};

FormatCapabilities GetFormatCapabilities(Format format)
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(ctx.physDevice, TranslateFormat(format), &formatProperties);

	FormatCapabilities capabilities = {};
	if (formatProperties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)
		capabilities |= FormatCapabilities::VertexAttribute;

	for (auto [vkCapability, capability] : imageFormatCapabilities)
	{
		if (formatProperties.optimalTilingFeatures & vkCapability)
			capabilities |= capability;
	}

	return capabilities;
}

void SetEnableVSync(bool enableVSync)
{
	ctx.swapchain.SetEnableVSync(enableVSync);
}

GraphicsMemoryStat GetMemoryStat()
{
	std::vector<VmaBudget> stats(ctx.memoryProperties.memoryHeapCount);
	vmaGetHeapBudgets(ctx.allocator, stats.data());

	VmaTotalStatistics vmaStats;
	vmaCalculateStatistics(ctx.allocator, &vmaStats);
	GraphicsMemoryStat stat = {};

	for (uint32_t h = 0; h < ctx.memoryProperties.memoryHeapCount; h++)
	{
		if (ctx.memoryProperties.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
		{
			stat.allocatedBytesGPU += stats[h].usage;
		}
		stat.allocatedBytes += stats[h].usage;
		stat.numBlocks += stats[h].statistics.blockCount;
	}

	return stat;
}

void DestroySamplers();

void Shutdown()
{
	vkDeviceWaitIdle(ctx.device);

	ProcessPendingInitBuffers(true);

	CachedDescriptorSetLayout::DestroyCached();
	DestroySamplers();
	DestroyRenderPasses();

	for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
	{
		VulkanCommandContext::immediateContexts[i].referencedResources.Release();
		vkDestroyFence(ctx.device, ctx.frameQueueFences[i], nullptr);
		vkDestroySemaphore(ctx.device, ctx.frameQueueSemaphores[i], nullptr);
	}

	vkDestroyCommandPool(ctx.device, ctx.mainCommandPool, nullptr);

	ctx.swapchain.Destroy();

	vmaDestroyAllocator(ctx.allocator);

	vkDestroyDevice(ctx.device, nullptr);

	vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);

	if (ctx.debugMessenger)
		vkDestroyDebugUtilsMessengerEXT(ctx.instance, ctx.debugMessenger, nullptr);

	vkDestroyInstance(ctx.instance, nullptr);
}

void EndLoading()
{
	VkCommandBuffer cb = VulkanCommandContext::immediateContexts[0].cb;
	vkEndCommandBuffer(cb);

	const VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cb,
	};

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
	if (acquireSemaphore == VK_NULL_HANDLE)
		acquireSemaphore = ctx.swapchain.AcquireImage();
}

void GetDrawableSize(int& width, int& height)
{
	width = ToInt(ctx.swapchain.m_surfaceExtent.width);
	height = ToInt(ctx.swapchain.m_surfaceExtent.height);
}

void BeginFrame()
{
	acquireSemaphore = VK_NULL_HANDLE;

	// Waits for the frame queue's fence to complete
	VkFence fence = ctx.frameQueueFences[CFrameIdx()];
	CheckRes(vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX));
	CheckRes(vkResetFences(ctx.device, 1, &fence));

	ProcessPendingInitBuffers(false);

	VulkanCommandContext::currentImmediate = &VulkanCommandContext::immediateContexts[CFrameIdx()];

	VulkanCommandContext::currentImmediate->referencedResources.Release();
	VulkanCommandContext::currentImmediate->SetInitialState();

	static const VkCommandBufferBeginInfo beginInfo = {
		/* sType            */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		/* pNext            */ nullptr,
		/* flags            */ VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		/* pInheritanceInfo */ nullptr
	};
	CheckRes(vkBeginCommandBuffer(VulkanCommandContext::currentImmediate->cb, &beginInfo));

	ctx.defaultFramebufferInPresentMode = true;
}

void EndFrame()
{
	MaybeAcquireSwapchainImage();

	VkCommandBuffer immediateCB = VulkanCommandContext::currentImmediate->cb;

	if (!ctx.defaultFramebufferInPresentMode)
	{
		VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.image = ctx.swapchain.CurrentImage();
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		vkCmdPipelineBarrier(
			immediateCB, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
			nullptr, 0, nullptr, 1, &barrier);
	}
	else
	{
		VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.image = ctx.swapchain.CurrentImage();
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		vkCmdPipelineBarrier(
			immediateCB, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
			1, &barrier);

		VkClearColorValue clearValue = {};
		VkImageSubresourceRange clearRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vkCmdClearColorImage(
			immediateCB, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &clearRange);

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = 0;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier(
			immediateCB, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
			nullptr, 1, &barrier);
	}

	CheckRes(vkEndCommandBuffer(immediateCB));

	const VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

	const VkSubmitInfo submitInfo = {
		/* sType                */ VK_STRUCTURE_TYPE_SUBMIT_INFO,
		/* pNext                */ nullptr,
		/* waitSemaphoreCount   */ 1,
		/* pWaitSemaphores      */ &acquireSemaphore,
		/* pWaitDstStageMask    */ &waitStages,
		/* commandBufferCount   */ 1,
		/* pCommandBuffers      */ &immediateCB,
		/* signalSemaphoreCount */ 1,
		/* pSignalSemaphores    */ &ctx.frameQueueSemaphores[CFrameIdx()],
	};

	VkResult presentResult;
	const VkPresentInfoKHR presentInfo = {
		/* sType              */ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		/* pNext              */ nullptr,
		/* waitSemaphoreCount */ 1,
		/* pWaitSemaphores    */ &ctx.frameQueueSemaphores[CFrameIdx()],
		/* swapchainCount     */ 1,
		/* pSwapchains        */ &ctx.swapchain.m_swapchain,
		/* pImageIndices      */ &ctx.swapchain.m_currentImage,
		/* pResults           */ &presentResult,
	};

	CheckRes(vkQueueSubmit(ctx.mainQueue, 1, &submitInfo, ctx.frameQueueFences[CFrameIdx()]));

	vkQueuePresentKHR(ctx.mainQueue, &presentInfo);

	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
	{
		ctx.swapchain.Create();
	}
	else
	{
		CheckRes(presentResult);
	}
}

FenceHandle CreateFence()
{
	VkFenceCreateInfo createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fence;
	CheckRes(vkCreateFence(ctx.device, &createInfo, nullptr, &fence));
	return reinterpret_cast<FenceHandle>(fence);
}

void DestroyFence(FenceHandle handle)
{
	vkDestroyFence(ctx.device, reinterpret_cast<VkFence>(handle), nullptr);
}

FenceStatus WaitForFence(FenceHandle _fence, uint64_t timeout)
{
	VkFence fence = reinterpret_cast<VkFence>(_fence);
	VkResult result = vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, timeout);
	switch (result)
	{
	case VK_TIMEOUT: return FenceStatus::Timeout;
	case VK_SUCCESS: return FenceStatus::Signaled;
	case VK_ERROR_DEVICE_LOST: return FenceStatus::Error;
	default: EG_PANIC("unexpected result from vkWaitForFences: " << result);
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
		vkCmdBeginDebugUtilsLabelEXT(UnwrapCC(cctx).cb, &labelInfo);
	}
}

void DebugLabelEnd(CommandContextHandle cctx)
{
	if (vkCmdEndDebugUtilsLabelEXT != nullptr)
	{
		vkCmdEndDebugUtilsLabelEXT(UnwrapCC(cctx).cb);
	}
}

void DebugLabelInsert(CommandContextHandle cctx, const char* label, const float* color)
{
	if (vkCmdInsertDebugUtilsLabelEXT != nullptr)
	{
		VkDebugUtilsLabelEXT labelInfo;
		InitLabelInfo(labelInfo, label, color);
		vkCmdInsertDebugUtilsLabelEXT(UnwrapCC(cctx).cb, &labelInfo);
	}
}
} // namespace eg::graphics_api::vk

#endif

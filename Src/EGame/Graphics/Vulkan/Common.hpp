#pragma once

#include <vulkan/vulkan_core.h>
#ifndef EG_NO_VULKAN

#include "../../Log.hpp"
#include "../Abstraction.hpp"
#include "../Graphics.hpp"
#include "VulkanMain.hpp"
#include "Swapchain.hpp"

#include <atomic>
#include <mutex>
#include <set>
#include <vk_mem_alloc.h>
#include <volk.h>

namespace eg
{
template <>
std::string LogToString(VkResult result);
}

namespace eg::graphics_api::vk
{
struct Resource
{
	std::atomic_int refCount;

	void UnRef()
	{
		if (--refCount <= 0)
			Free();
	}

	virtual void Free() = 0;
};

class ReferencedResourceSet
{
public:
	ReferencedResourceSet() = default;

	~ReferencedResourceSet() { Release(); }

	void Add(Resource& resource);

	void Remove(Resource& resource);

	void Release();

private:
	std::set<Resource*> m_resources;
};

struct Context
{
	bool hasDebugUtils;
	bool hasPushDescriptorExtension;

	VkInstance instance;
	VkSurfaceKHR surface;

	// ** Device related fields **
	uint32_t queueFamily;
	VkQueueFamilyProperties queueFamilyProperties;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkPhysicalDeviceLimits deviceLimits;
	VkPhysicalDeviceFeatures deviceFeatures;
	VkPhysicalDevice physDevice;
	bool hasDynamicStatePolygonMode = false;
	std::string deviceName;
	std::string_view deviceVendorName;
	VkDevice device;
	VkQueue mainQueue;
	VkDebugUtilsMessengerEXT debugMessenger;
	VmaAllocator allocator;
	
	SubgroupFeatures subgroupFeatures;

	VkCommandPool mainCommandPool;
	
	Swapchain swapchain;

	VkImage defaultDSImage;
	VmaAllocation defaultDSImageAllocation;
	VkImageView defaultDSImageView;
	VkFramebuffer defaultFramebuffers[16];
	VkFormat defaultDSFormat;
	bool defaultFramebufferInPresentMode;

	// ** Frame queue related fields **
	VkSemaphore frameQueueSemaphores[MAX_CONCURRENT_FRAMES];
	VkFence frameQueueFences[MAX_CONCURRENT_FRAMES];
};

extern Context ctx;

template <typename Root, typename Ext>
inline void PushPNext(Root& root, Ext& ext)
{
	ext.pNext = const_cast<void*>(root.pNext);
	root.pNext = &ext;
}

inline bool HasStencil(VkFormat format)
{
	return format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT ||
	       format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*);

void SetObjectName(uint64_t objectHandle, VkObjectType objectType, const char* name);

void CheckRes(VkResult result);

VkImageAspectFlags GetFormatAspect(Format format);
VkFormat RelaxDepthStencilFormat(VkFormat format);

inline VkSemaphore CreateSemaphore(VkDevice device)
{
	static const VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkSemaphore semaphore;
	CheckRes(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore));
	return semaphore;
}

inline VkFence CreateFence(VkDevice device)
{
	static const VkFenceCreateInfo createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fence;
	CheckRes(vkCreateFence(device, &createInfo, nullptr, &fence));
	return fence;
}
} // namespace eg::graphics_api::vk

#endif

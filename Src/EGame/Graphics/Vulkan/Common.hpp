#pragma once

#ifndef EG_NO_VULKAN

#include "../Graphics.hpp"
#include "../Abstraction.hpp"
#include "../../Log.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>
#include <atomic>
#include <set>
#include <mutex>

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
		
		~ReferencedResourceSet()
		{
			Release();
		}
		
		void Add(Resource& resource);
		
		void Remove(Resource& resource);
		
		void Release();
		
	private:
		std::set<Resource*> m_resources;
	};
	
	struct CommandContextState
	{
		float viewportX;
		float viewportY;
		float viewportW;
		float viewportH;
		VkRect2D scissor;
		bool viewportOutOfDate;
		bool scissorOutOfDate;
		struct AbstractPipeline* pipeline;
		uint32_t framebufferW;
		uint32_t framebufferH;
	};
	
	struct Context
	{
		bool hasDebugUtils;
		VkInstance instance;
		VkSurfaceKHR surface;
		
		// ** Device related fields **
		uint32_t queueFamily;
		VkQueueFamilyProperties queueFamilyProperties;
		VkPhysicalDeviceMemoryProperties memoryProperties;
		VkPhysicalDeviceLimits deviceLimits;
		VkPhysicalDeviceFeatures deviceFeatures;
		VkPhysicalDevice physDevice;
		VkDevice device;
		VkQueue mainQueue;
		VkQueue backgroundQueue;
		VkDebugUtilsMessengerEXT debugMessenger;
		VmaAllocator allocator;
		
		VkCommandPool mainCommandPool;
		
		// ** Swapchain related fields **
		VkSurfaceFormatKHR surfaceFormat;
		VkExtent2D surfaceExtent;
		VkPresentModeKHR presentMode;
		VkSwapchainKHR swapchain;
		uint32_t numSwapchainImages;
		VkImage swapchainImages[16];
		VkImageView swapchainImageViews[16];
		uint32_t acquireSemaphoreIndex;
		VkSemaphore acquireSemaphores[16];
		
		VkImage defaultDSImage;
		VmaAllocation defaultDSImageAllocation;
		VkImageView defaultDSImageView;
		VkFramebuffer defaultFramebuffers[16];
		VkFormat defaultDSFormat;
		bool defaultFramebufferInPresentMode;
		
		// ** Frame queue related fields **
		VkSemaphore frameQueueSemaphores[MAX_CONCURRENT_FRAMES];
		VkFence frameQueueFences[MAX_CONCURRENT_FRAMES];
		VkCommandBuffer immediateCommandBuffers[MAX_CONCURRENT_FRAMES];
		ReferencedResourceSet referencedResources[MAX_CONCURRENT_FRAMES];
		
		CommandContextState immediateCCState;
		
		uint32_t currentImage;
	};
	
	extern Context ctx;
	
	inline VkCommandBuffer GetCB(CommandContextHandle handle)
	{
		return handle == nullptr ? ctx.immediateCommandBuffers[CFrameIdx()] : reinterpret_cast<VkCommandBuffer>(handle);
	}
	
	inline CommandContextState& GetCtxState(CommandContextHandle handle)
	{
		return ctx.immediateCCState;
	}
	
	inline void RefResource(CommandContextHandle handle, Resource& resource)
	{
		if (handle == nullptr)
			ctx.referencedResources[CFrameIdx()].Add(resource);
	}
	
	inline bool HasStencil(VkFormat format)
	{
		return format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT ||
		       format == VK_FORMAT_D32_SFLOAT_S8_UINT;
	}
	
	VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*);
	
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
}

#endif

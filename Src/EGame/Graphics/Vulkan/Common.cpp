#include "Common.hpp"

namespace eg
{
	template <>
	std::string LogToString(VkResult result)
	{
		switch (result)
		{
#define RESULT_TYPE(T) case T: return #T;
		RESULT_TYPE(VK_SUCCESS)
		RESULT_TYPE(VK_NOT_READY)
		RESULT_TYPE(VK_TIMEOUT)
		RESULT_TYPE(VK_EVENT_SET)
		RESULT_TYPE(VK_EVENT_RESET)
		RESULT_TYPE(VK_INCOMPLETE)
		RESULT_TYPE(VK_ERROR_OUT_OF_HOST_MEMORY)
		RESULT_TYPE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
		RESULT_TYPE(VK_ERROR_INITIALIZATION_FAILED)
		RESULT_TYPE(VK_ERROR_DEVICE_LOST)
		RESULT_TYPE(VK_ERROR_MEMORY_MAP_FAILED)
		RESULT_TYPE(VK_ERROR_LAYER_NOT_PRESENT)
		RESULT_TYPE(VK_ERROR_EXTENSION_NOT_PRESENT)
		RESULT_TYPE(VK_ERROR_FEATURE_NOT_PRESENT)
		RESULT_TYPE(VK_ERROR_INCOMPATIBLE_DRIVER)
		RESULT_TYPE(VK_ERROR_TOO_MANY_OBJECTS)
		RESULT_TYPE(VK_ERROR_FORMAT_NOT_SUPPORTED)
		RESULT_TYPE(VK_ERROR_FRAGMENTED_POOL)
		RESULT_TYPE(VK_ERROR_OUT_OF_POOL_MEMORY)
		RESULT_TYPE(VK_ERROR_INVALID_EXTERNAL_HANDLE)
		RESULT_TYPE(VK_ERROR_SURFACE_LOST_KHR)
		RESULT_TYPE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
		RESULT_TYPE(VK_SUBOPTIMAL_KHR)
		RESULT_TYPE(VK_ERROR_OUT_OF_DATE_KHR)
		RESULT_TYPE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
		RESULT_TYPE(VK_ERROR_VALIDATION_FAILED_EXT)
		RESULT_TYPE(VK_ERROR_INVALID_SHADER_NV)
		RESULT_TYPE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
		RESULT_TYPE(VK_ERROR_FRAGMENTATION_EXT)
		RESULT_TYPE(VK_ERROR_NOT_PERMITTED_EXT)
		RESULT_TYPE(VK_ERROR_INVALID_DEVICE_ADDRESS_EXT)
#undef RESULT_TYPE
		default: return "Unknown";
		}
	}
}

namespace eg::graphics_api::vk
{
	inline const char* GetObjectTypeName(VkObjectType type)
	{
		switch (type)
		{
		case VK_OBJECT_TYPE_INSTANCE:
			return "Instance";
		case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
			return "PhysicalDevice";
		case VK_OBJECT_TYPE_DEVICE:
			return "Device";
		case VK_OBJECT_TYPE_QUEUE:
			return "Queue";
		case VK_OBJECT_TYPE_SEMAPHORE:
			return "Semaphore";
		case VK_OBJECT_TYPE_COMMAND_BUFFER:
			return "CommandBuffer";
		case VK_OBJECT_TYPE_FENCE:
			return "Fence";
		case VK_OBJECT_TYPE_DEVICE_MEMORY:
			return "DeviceMemory";
		case VK_OBJECT_TYPE_BUFFER:
			return "Buffer";
		case VK_OBJECT_TYPE_IMAGE:
			return "Image";
		case VK_OBJECT_TYPE_EVENT:
			return "Event";
		case VK_OBJECT_TYPE_QUERY_POOL:
			return "QueryPool";
		case VK_OBJECT_TYPE_BUFFER_VIEW:
			return "BufferView";
		case VK_OBJECT_TYPE_IMAGE_VIEW:
			return "ImageView";
		case VK_OBJECT_TYPE_SHADER_MODULE:
			return "ShaderModule";
		case VK_OBJECT_TYPE_PIPELINE_CACHE:
			return "PipelineCache";
		case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
			return "PipelineLayout";
		case VK_OBJECT_TYPE_RENDER_PASS:
			return "RenderPass";
		case VK_OBJECT_TYPE_PIPELINE:
			return "Pipeline";
		case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
			return "DescriptorSetLayout";
		case VK_OBJECT_TYPE_SAMPLER:
			return "Sampler";
		case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
			return "DescriptorPool";
		case VK_OBJECT_TYPE_DESCRIPTOR_SET:
			return "DescriptorSet";
		case VK_OBJECT_TYPE_FRAMEBUFFER:
			return "Framebuffer";
		case VK_OBJECT_TYPE_COMMAND_POOL:
			return "CommandPool";
		default:
			return "Unknown";
		}
	}
	
	inline void PrintAffectedObjects(const VkDebugUtilsMessengerCallbackDataEXT& callbackData, std::ostream& stream)
	{
		if (callbackData.objectCount == 0)
			return;
		
		stream << "Affected Objects:" << std::endl;
		for (uint32_t i = 0; i < callbackData.objectCount; i++)
		{
			stream << " - 0x" << std::hex << callbackData.pObjects[i].objectHandle << " ";
			
			if (callbackData.pObjects[i].pObjectName)
				stream << "\"" << callbackData.pObjects[i].pObjectName << "\"";
			else
				stream << "-";
			
			stream << " (" << GetObjectTypeName(callbackData.pObjects[i].objectType) << ")" << std::endl;
		}
	}
	
	VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*)
	{
		if (std::strcmp(callbackData->pMessageIdName, "UNASSIGNED-CoreValidation-Shader-InconsistentSpirv") == 0)
			return VK_FALSE;
		
		if (std::strstr(callbackData->pMessage, "Mapping an image with layout VK_IMAGE_LAYOUT_UNDEFINED can result in undefined behavior"))
			return VK_FALSE;
		
		std::cerr << "Vk[" << callbackData->messageIdNumber << " " << callbackData->pMessageIdName << "]: \n" <<
			callbackData->pMessage << std::endl;
		
		PrintAffectedObjects(*callbackData, std::cerr);
		
#ifndef NDEBUG
		if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			EG_DEBUG_BREAK
			std::abort();
		}
#endif
		
		return VK_FALSE;
	}
	
	void SetObjectName(uint64_t objectHandle, VkObjectType objectType, const char* name)
	{
		if (!ctx.hasDebugUtils)
			return;
		
		VkDebugUtilsObjectNameInfoEXT nameInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
		nameInfo.objectHandle = objectHandle;
		nameInfo.objectType = objectType;
		nameInfo.pObjectName = name;
		
		vkSetDebugUtilsObjectNameEXT(ctx.device, &nameInfo);
	}
	
	void CheckRes(VkResult result)
	{
		switch (result)
		{
		#define ERROR_TYPE(name) case VK_ERROR_ ## name: EG_PANIC("Vulkan error " #name);
		ERROR_TYPE(OUT_OF_HOST_MEMORY)
		ERROR_TYPE(OUT_OF_DEVICE_MEMORY)
		ERROR_TYPE(INITIALIZATION_FAILED)
		ERROR_TYPE(DEVICE_LOST)
		ERROR_TYPE(MEMORY_MAP_FAILED)
		ERROR_TYPE(LAYER_NOT_PRESENT)
		ERROR_TYPE(EXTENSION_NOT_PRESENT)
		ERROR_TYPE(FEATURE_NOT_PRESENT)
		ERROR_TYPE(INCOMPATIBLE_DRIVER)
		ERROR_TYPE(TOO_MANY_OBJECTS)
		ERROR_TYPE(FORMAT_NOT_SUPPORTED)
		ERROR_TYPE(FRAGMENTED_POOL)
		ERROR_TYPE(OUT_OF_POOL_MEMORY)
		ERROR_TYPE(INVALID_EXTERNAL_HANDLE)
		ERROR_TYPE(SURFACE_LOST_KHR)
		ERROR_TYPE(NATIVE_WINDOW_IN_USE_KHR)
		ERROR_TYPE(OUT_OF_DATE_KHR)
		ERROR_TYPE(INCOMPATIBLE_DISPLAY_KHR)
		ERROR_TYPE(VALIDATION_FAILED_EXT)
		ERROR_TYPE(INVALID_SHADER_NV)
		ERROR_TYPE(FRAGMENTATION_EXT)
		ERROR_TYPE(NOT_PERMITTED_EXT)
		ERROR_TYPE(INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
		ERROR_TYPE(INVALID_DEVICE_ADDRESS_EXT)
		default: break;
		}
	}
	
	VkFormat TranslateFormat(Format format)
	{
		switch (format)
		{
		case Format::Undefined: return VK_FORMAT_UNDEFINED;
		case Format::DefaultColor: return ctx.surfaceFormat.format;
		case Format::DefaultDepthStencil: return ctx.defaultDSFormat;
		case Format::R8_UNorm: return VK_FORMAT_R8_UNORM;
		case Format::R8_UInt: return VK_FORMAT_R8_UINT;
		case Format::R8_SInt: return VK_FORMAT_R8_SINT;
		case Format::R16_UInt: return VK_FORMAT_R16_UINT;
		case Format::R16_SInt: return VK_FORMAT_R16_SINT;
		case Format::R16_Float: return VK_FORMAT_R16_SFLOAT;
		case Format::R32_UInt: return VK_FORMAT_R32_UINT;
		case Format::R32_SInt: return VK_FORMAT_R32_SINT;
		case Format::R32_Float: return VK_FORMAT_R32_SFLOAT;
		case Format::R8G8_UNorm: return VK_FORMAT_R8G8_UNORM;
		case Format::R8G8_UInt: return VK_FORMAT_R8G8_UINT;
		case Format::R8G8_SInt: return VK_FORMAT_R8G8_SINT;
		case Format::R16G16_UInt: return VK_FORMAT_R16G16_UINT;
		case Format::R16G16_SInt: return VK_FORMAT_R16G16_SINT;
		case Format::R16G16_Float: return VK_FORMAT_R16G16_SFLOAT;
		case Format::R32G32_UInt: return VK_FORMAT_R32G32_UINT;
		case Format::R32G32_SInt: return VK_FORMAT_R32G32_SINT;
		case Format::R32G32_Float: return VK_FORMAT_R32G32_SFLOAT;
		case Format::R8G8B8_sRGB: return VK_FORMAT_R8G8B8_SRGB;
		case Format::R16G16B16_UInt: return VK_FORMAT_R16G16B16_UINT;
		case Format::R16G16B16_SInt: return VK_FORMAT_R16G16B16_SINT;
		case Format::R16G16B16_Float: return VK_FORMAT_R16G16B16_SFLOAT;
		case Format::R32G32B32_UInt: return VK_FORMAT_R32G32B32_UINT;
		case Format::R32G32B32_SInt: return VK_FORMAT_R32G32B32_SINT;
		case Format::R32G32B32_Float: return VK_FORMAT_R32G32B32_SFLOAT;
		case Format::R8G8B8A8_sRGB: return VK_FORMAT_R8G8B8A8_SRGB;
		case Format::R8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
		case Format::R8G8B8A8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
		case Format::R8G8B8A8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
		case Format::R16G16B16A16_UInt: return VK_FORMAT_R16G16B16A16_UINT;
		case Format::R16G16B16A16_SInt: return VK_FORMAT_R16G16B16A16_SINT;
		case Format::R16G16B16A16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case Format::R32G32B32A32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
		case Format::R32G32B32A32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
		case Format::R32G32B32A32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case Format::BC1_RGBA_UNorm: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case Format::BC1_RGBA_sRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		case Format::BC1_RGB_UNorm: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case Format::BC1_RGB_sRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		case Format::BC3_UNorm: return VK_FORMAT_BC3_UNORM_BLOCK;
		case Format::BC3_sRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
		case Format::BC4_UNorm: return VK_FORMAT_BC4_UNORM_BLOCK;
		case Format::BC5_UNorm: return VK_FORMAT_BC5_UNORM_BLOCK;
		case Format::Depth16: return VK_FORMAT_D16_UNORM;
		case Format::Depth32: return VK_FORMAT_D32_SFLOAT;
		case Format::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
		case Format::Depth32Stencil8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
		}
		EG_UNREACHABLE
	}
	
	VkImageAspectFlags GetFormatAspect(Format format)
	{
		if (format == Format::Depth16 || format == Format::Depth32)
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		if (format == Format::Depth24Stencil8 || format == Format::Depth32Stencil8)
			return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		return VK_IMAGE_ASPECT_COLOR_BIT;
	}
	
	VkFormat RelaxDepthStencilFormat(VkFormat format)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(ctx.physDevice, format, &formatProperties);
		
		VkFormatFeatureFlags featureFlags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
		
		if ((formatProperties.optimalTilingFeatures & featureFlags) == featureFlags)
			return format;
		
		if (format == VK_FORMAT_D32_SFLOAT)
			return VK_FORMAT_D16_UNORM;
		
		const VkFormat depthStencilFormats[] =
		{
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT
		};
		
		if (Contains(depthStencilFormats, format))
		{
			for (VkFormat dsFormat : depthStencilFormats)
			{
				vkGetPhysicalDeviceFormatProperties(ctx.physDevice, dsFormat, &formatProperties);
				if ((formatProperties.optimalTilingFeatures & featureFlags) == featureFlags)
					return dsFormat;
			}
		}
		
		EG_PANIC("Unable to select a supported depth stencil format.");
	}
	
	void ReferencedResourceSet::Add(Resource& resource)
	{
		auto it = std::lower_bound(m_resources.begin(), m_resources.end(), &resource);
		if (it != m_resources.end() && *it == &resource)
			return;
		resource.refCount++;
		m_resources.insert(it, &resource);
	}
	
	void ReferencedResourceSet::Release()
	{
		for (Resource* resource : m_resources)
			resource->UnRef();
		m_resources.clear();
	}
}
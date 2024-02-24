#ifndef EG_NO_VULKAN

#include "Common.hpp"
#include "../../Assert.hpp"

#include <cstring>

namespace eg
{
template <>
std::string LogToString(VkResult result)
{
	return std::to_string(result);
}
} // namespace eg

namespace eg::graphics_api::vk
{
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

		stream << " (" << static_cast<int>(callbackData.pObjects[i].objectType) << ")" << std::endl;
	}
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*)
{
	const char* messageIdName = callbackData->pMessageIdName;
	if (std::strstr(messageIdName, "CoreValidation-DrawState-InvalidCommandBuffer-VkDescriptorSet"))
		return VK_FALSE;
	if (std::strstr(messageIdName, "CoreValidation-Shader-OutputNotConsumed"))
		return VK_FALSE;
	if (std::strstr(messageIdName, "vkDestroyDevice"))
		return VK_FALSE;

	std::cerr << "Vk[" << callbackData->messageIdNumber << " " << callbackData->pMessageIdName << "]: \n"
			  << callbackData->pMessage << std::endl;

	PrintAffectedObjects(*callbackData, std::cerr);

	if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		EG_DEBUG_BREAK
		std::abort();
	}

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
#define ERROR_TYPE(name)                                                                                               \
	case name: EG_PANIC("Vulkan error " #name); break;
		ERROR_TYPE(VK_ERROR_OUT_OF_HOST_MEMORY)
		ERROR_TYPE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
		ERROR_TYPE(VK_ERROR_INITIALIZATION_FAILED)
		ERROR_TYPE(VK_ERROR_DEVICE_LOST)
		ERROR_TYPE(VK_ERROR_MEMORY_MAP_FAILED)
		ERROR_TYPE(VK_ERROR_LAYER_NOT_PRESENT)
		ERROR_TYPE(VK_ERROR_EXTENSION_NOT_PRESENT)
		ERROR_TYPE(VK_ERROR_FEATURE_NOT_PRESENT)
		ERROR_TYPE(VK_ERROR_INCOMPATIBLE_DRIVER)
		ERROR_TYPE(VK_ERROR_TOO_MANY_OBJECTS)
		ERROR_TYPE(VK_ERROR_FORMAT_NOT_SUPPORTED)
		ERROR_TYPE(VK_ERROR_FRAGMENTED_POOL)
		ERROR_TYPE(VK_ERROR_OUT_OF_POOL_MEMORY)
		ERROR_TYPE(VK_ERROR_INVALID_EXTERNAL_HANDLE)
		ERROR_TYPE(VK_ERROR_SURFACE_LOST_KHR)
		ERROR_TYPE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
		ERROR_TYPE(VK_ERROR_OUT_OF_DATE_KHR)
		ERROR_TYPE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
		ERROR_TYPE(VK_ERROR_VALIDATION_FAILED_EXT)
		ERROR_TYPE(VK_ERROR_INVALID_SHADER_NV)
		ERROR_TYPE(VK_ERROR_FRAGMENTATION_EXT)
		ERROR_TYPE(VK_ERROR_NOT_PERMITTED_EXT)
		ERROR_TYPE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
		ERROR_TYPE(VK_ERROR_INVALID_DEVICE_ADDRESS_EXT)
	default: break;
	}
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

	VkFormatFeatureFlags featureFlags =
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

	if ((formatProperties.optimalTilingFeatures & featureFlags) == featureFlags)
		return format;

	if (format == VK_FORMAT_D32_SFLOAT)
		return VK_FORMAT_D16_UNORM;

	const VkFormat depthStencilFormats[] = { VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
		                                     VK_FORMAT_D32_SFLOAT_S8_UINT };

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
	if (m_resources.insert(&resource).second)
	{
		resource.refCount++;
	}
}

void ReferencedResourceSet::Release()
{
	for (Resource* resource : m_resources)
		resource->UnRef();
	m_resources.clear();
}

void ReferencedResourceSet::Remove(Resource& resource)
{
	if (m_resources.erase(&resource))
	{
		resource.UnRef();
	}
}
} // namespace eg::graphics_api::vk

#endif

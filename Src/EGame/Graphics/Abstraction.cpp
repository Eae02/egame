#include "Abstraction.hpp"
#include "../Assert.hpp"
#include "../Hash.hpp"
#include "../Log.hpp"
#include "OpenGL/OpenGL.hpp"
#include "Vulkan/VulkanMain.hpp"

#ifdef __APPLE__
#include "Metal/MetalMain.hpp"
#endif

namespace eg
{
size_t SamplerDescription::Hash() const
{
	size_t h = 0;
	h |= static_cast<size_t>(wrapU) << 0;
	h |= static_cast<size_t>(wrapV) << 2;
	h |= static_cast<size_t>(wrapW) << 4;
	h |= static_cast<size_t>(minFilter) << 6;
	h |= static_cast<size_t>(magFilter) << 7;
	h |= static_cast<size_t>(mipFilter) << 8;
	h |= static_cast<size_t>(borderColor) << 9;
	h |= static_cast<size_t>(enableCompare) << 12;
	h |= static_cast<size_t>(compareOp) << 13;

	HashAppend(h, maxAnistropy);
	HashAppend(h, mipLodBias);

	return h;
}

size_t DescriptorSetBinding::Hash() const
{
	size_t h = 0;
	h |= static_cast<size_t>(type);
	h |= static_cast<size_t>(shaderAccess) << 3;
	h |= static_cast<size_t>(rwMode) << 15;
	h |= static_cast<size_t>(count) << 18;
	h |= static_cast<size_t>(binding) << 32;
	return h;
}

namespace gal
{
GraphicsMemoryStat (*GetMemoryStat)();

#define XM_ABSCALLBACK(name, ret, params) ret(*name) params;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
} // namespace gal

GraphicsAPI detail::graphicsAPI;
std::string_view detail::graphicsAPIName;

bool InitializeGraphicsAPI(GraphicsAPI api, const GraphicsAPIInitArguments& initArguments)
{
	detail::graphicsAPI = api;

	switch (api)
	{
	case GraphicsAPI::OpenGL:
		if (initArguments.preferGLESPath)
			detail::graphicsAPIName = "GLES3";
		else
			detail::graphicsAPIName = "GL4";
		break;
	case GraphicsAPI::Vulkan: detail::graphicsAPIName = "Vulkan"; break;
	case GraphicsAPI::Metal: detail::graphicsAPIName = "Metal"; break;
	}

	switch (api)
	{
	case GraphicsAPI::OpenGL:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::gl::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
		return eg::graphics_api::gl::Initialize(initArguments);

#ifndef EG_NO_VULKAN
	case GraphicsAPI::Vulkan:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::vk::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
		gal::GetMemoryStat = &eg::graphics_api::vk::GetMemoryStat;
		return eg::graphics_api::vk::Initialize(initArguments);
#endif

#ifdef __APPLE__
	case GraphicsAPI::Metal:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::mtl::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
		return eg::graphics_api::mtl::Initialize(initArguments);
#endif

	default: break;
	}

	return false;
}

void DestroyGraphicsAPI()
{
	gal::Shutdown();

#define XM_ABSCALLBACK(name, ret, params) gal::name = nullptr;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
}

template <>
std::string LogToString(UniformType type)
{
	switch (type)
	{
	case UniformType::Int: return "Int";
	case UniformType::Float: return "Float";
	case UniformType::Vec2: return "Vec2";
	case UniformType::Vec3: return "Vec3";
	case UniformType::Vec4: return "Vec4";
	case UniformType::IVec2: return "IVec2";
	case UniformType::IVec3: return "IVec3";
	case UniformType::IVec4: return "IVec4";
	case UniformType::Mat3: return "Mat3";
	case UniformType::Mat4: return "Mat4";
	}
	EG_UNREACHABLE
}

std::string_view BindingTypeToString(BindingType bindingType)
{
	switch (bindingType)
	{
	case BindingType::UniformBuffer: return "UniformBuffer";
	case BindingType::StorageBuffer: return "StorageBuffer";
	case BindingType::Texture: return "Texture";
	case BindingType::StorageImage: return "StorageImage";
	}
	EG_UNREACHABLE
}

TextureSubresource TextureSubresource::ResolveRem(uint32_t maxMipLevels, uint32_t maxArrayLayers) const
{
	TextureSubresource result = *this;
	if (numMipLevels == REMAINING_SUBRESOURCE)
		result.numMipLevels = maxMipLevels - firstMipLevel;
	if (numArrayLayers == REMAINING_SUBRESOURCE)
		result.numArrayLayers = maxArrayLayers - firstArrayLayer;
	return result;
}

TextureSubresourceLayers TextureSubresourceLayers::ResolveRem(uint32_t maxArrayLayers) const
{
	TextureSubresourceLayers result = *this;
	if (numArrayLayers == REMAINING_SUBRESOURCE)
		result.numArrayLayers = maxArrayLayers - firstArrayLayer;
	return result;
}

size_t TextureSubresource::Hash() const
{
	size_t h = 0;
	HashAppend(h, firstMipLevel);
	HashAppend(h, numMipLevels);
	HashAppend(h, firstArrayLayer);
	HashAppend(h, numArrayLayers);
	return h;
}

size_t TextureSubresourceLayers::Hash() const
{
	size_t h = 0;
	HashAppend(h, mipLevel);
	HashAppend(h, firstArrayLayer);
	HashAppend(h, numArrayLayers);
	return h;
}
} // namespace eg

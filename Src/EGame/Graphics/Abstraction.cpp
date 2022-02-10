#include "Abstraction.hpp"
#include "OpenGL/OpenGL.hpp"
#include "Vulkan/VulkanMain.hpp"
#include "../Log.hpp"
#include "../Hash.hpp"
#include "../Assert.hpp"

bool eg::SamplerDescription::operator==(const eg::SamplerDescription& rhs) const
{
	return wrapU == rhs.wrapU &&
		wrapV == rhs.wrapV &&
		wrapW == rhs.wrapW &&
		minFilter == rhs.minFilter &&
		magFilter == rhs.magFilter &&
		mipFilter == rhs.mipFilter &&
		mipLodBias == rhs.mipLodBias &&
		maxAnistropy == rhs.maxAnistropy &&
		borderColor == rhs.borderColor;
}

bool eg::SamplerDescription::operator!=(const eg::SamplerDescription& rhs) const
{
	return !(rhs == *this);
}

namespace eg
{
	namespace gal
	{
		GraphicsMemoryStat (*GetMemoryStat)();
		
#define XM_ABSCALLBACK(name, ret, params) ret (*name)params;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
	}
	
	GraphicsAPI detail::graphicsAPI;
	
	bool InitializeGraphicsAPI(GraphicsAPI api, const GraphicsAPIInitArguments& initArguments)
	{
		detail::graphicsAPI = api;
		
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
			
		default:
			break;
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
}

#include "Abstraction.hpp"
#include "OpenGL/OpenGL.hpp"
#include "Vulkan/VulkanMain.hpp"

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
#define XM_ABSCALLBACK(name, ret, params) ret (*name)params;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
	}
	
	bool InitializeGraphicsAPI(GraphicsAPI api, const GraphicsAPIInitArguments& initArguments)
	{
		switch (api)
		{
		case GraphicsAPI::OpenGL:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::gl::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
			return eg::graphics_api::gl::Initialize(initArguments);
		case GraphicsAPI::Vulkan:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::vk::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
			return eg::graphics_api::vk::Initialize(initArguments);
		}
		
		return false;
	}
	
	void DestroyGraphicsAPI()
	{
		gal::Shutdown();
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
}

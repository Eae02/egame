#include "Abstraction.hpp"
#include "OpenGL/OpenGL.hpp"

namespace eg
{
	namespace gal
	{
#define XM_ABSCALLBACK(name, ret, params) ret (*name)params;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
	}
	
	bool InitializeGraphicsAPI(GraphicsAPI api, SDL_Window* window)
	{
		switch (api)
		{
		case GraphicsAPI::OpenGL45:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::gl::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
			return eg::graphics_api::gl::Initialize(window);
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

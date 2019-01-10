#pragma once

#include "../Utils.hpp"

namespace eg
{
	enum class GraphicsAPI
	{
		OpenGL45,
		
#if defined(__linux__)
		Preferred = OpenGL45
#elif defined(_WIN32)
		Preferred = OpenGL45
#endif
	};
	
	namespace detail
	{
		extern EG_API uint32_t cFrameIdx;
		extern EG_API int resolutionX;
		extern EG_API int resolutionY;
	}
	
	inline uint32_t CFrameIdx()
	{
		return detail::cFrameIdx;
	}
	
	inline int CurrentResolutionX()
	{
		return detail::resolutionX;
	}
	
	inline int CurrentResolutionY()
	{
		return detail::resolutionY;
	}
	
	constexpr uint32_t MAX_CONCURRENT_FRAMES = 3;
}

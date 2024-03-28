#pragma once

#include <cstdint>

#include "../API.hpp"

namespace eg
{
enum class GraphicsAPI
{
	OpenGL,
	Vulkan,
	Metal,
	WebGPU,

#if defined(__APPLE__)
	Preferred = Metal,
#elif defined(__EMSCRIPTEN__)
	Preferred = WebGPU,
#else
	Preferred = Vulkan,
#endif
};

namespace detail
{
EG_API extern uint32_t cFrameIdx;
EG_API extern int resolutionX;
EG_API extern int resolutionY;
} // namespace detail

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

constexpr uint32_t MAX_COLOR_ATTACHMENTS = 8;
constexpr uint32_t MAX_DESCRIPTOR_SETS = 4;
constexpr uint32_t MAX_CONCURRENT_FRAMES = 3;
} // namespace eg

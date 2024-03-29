#pragma once

#include <cstdint>

#include "../API.hpp"

namespace eg
{
enum class GraphicsAPI
{
	OpenGL,
	Vulkan,
	Preferred = OpenGL
};

namespace detail
{
extern EG_API uint32_t cFrameIdx;
extern EG_API int resolutionX;
extern EG_API int resolutionY;
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

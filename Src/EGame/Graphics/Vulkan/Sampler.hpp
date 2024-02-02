#pragma once

#ifndef EG_NO_VULKAN

#include "Common.hpp"

namespace eg::graphics_api::vk
{
void DestroySamplers();

VkSampler GetSampler(const SamplerDescription& description);
} // namespace eg::graphics_api::vk

#endif

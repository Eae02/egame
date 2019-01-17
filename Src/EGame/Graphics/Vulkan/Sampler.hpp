#pragma once

#include "Common.hpp"

namespace eg::graphics_api::vk
{
	void DestroySamplers();
	
	VkSampler GetSampler(const SamplerDescription& description);
}

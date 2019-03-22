#pragma once

#include "Common.hpp"
#include "../Abstraction.hpp"

namespace eg::graphics_api::vk
{
	struct FramebufferFormat
	{
		VkSampleCountFlags sampleCount;
		VkFormat depthStencilFormat;
		VkFormat colorFormats[MAX_COLOR_ATTACHMENTS];
		size_t hash;
		
		static FramebufferFormat FromHint(const FramebufferFormatHint& hint);
		
		void CalcHash();
	};
	
	extern FramebufferFormat currentFBFormat;
}

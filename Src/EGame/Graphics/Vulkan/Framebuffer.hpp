#pragma once

#ifndef EG_NO_VULKAN

#include "../Abstraction.hpp"
#include "Common.hpp"

namespace eg::graphics_api::vk
{
struct FramebufferFormat
{
	VkSampleCountFlags sampleCount;
	VkFormat depthStencilFormat;
	VkFormat colorFormats[MAX_COLOR_ATTACHMENTS];
	eg::Format originalDepthStencilFormat;
	eg::Format originalColorFormats[MAX_COLOR_ATTACHMENTS];
	size_t hash;

	static FramebufferFormat FromHint(const FramebufferFormatHint& hint);

	void CalcHash();
};

extern FramebufferFormat currentFBFormat;
} // namespace eg::graphics_api::vk

#endif

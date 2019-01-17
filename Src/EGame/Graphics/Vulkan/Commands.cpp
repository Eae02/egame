#include "Common.hpp"

namespace eg::graphics_api::vk
{
	void SetViewport(CommandContextHandle cc, float x, float y, float w, float h)
	{
		const VkViewport viewport = { x, y + h, w, -h, 0.0f, 1.0f };
		vkCmdSetViewport(GetCB(cc), 0, 1, &viewport);
	}
	
	void SetScissor(CommandContextHandle cc, int x, int y, int w, int h)
	{
		const VkRect2D scissor = { { x, y }, { (uint32_t)w, (uint32_t)h } };
		vkCmdSetScissor(GetCB(cc), 0, 1, &scissor);
	}
}

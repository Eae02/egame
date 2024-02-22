#include "VulkanCommandContext.hpp"
#include "Pipeline.hpp"

#include <cstring>

namespace eg::graphics_api::vk
{
VulkanCommandContext* VulkanCommandContext::currentImmediate;
std::vector<VulkanCommandContext> VulkanCommandContext::immediateContexts;

void VulkanCommandContext::SetInitialState()
{
	viewportX = 0;
	viewportY = 0;
	viewportW = 0;
	viewportH = 0;
	scissor = {};
	viewportOutOfDate = true;
	scissorOutOfDate = true;

	polygonMode = VK_POLYGON_MODE_FILL;
	polygonModeOutOfDate = true;
	enableDynamicPolygonMode = false;

	cullMode = VK_CULL_MODE_NONE;
	cullModeOutOfDate = true;
	enableDynamicCullMode = false;

	pipeline = nullptr;
	framebufferW = 0;
	framebufferH = 0;

	m_pushDescriptorInfoAllocator.Reset();
	for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
		m_pendingPushDescriptorWrites[i].clear();
}

void VulkanCommandContext::FlushDynamicState()
{
	if (viewportOutOfDate)
	{
		const VkViewport viewport = { viewportX, viewportY + viewportH, viewportW, -viewportH, 0.0f, 1.0f };
		vkCmdSetViewport(cb, 0, 1, &viewport);
		viewportOutOfDate = false;
	}

	if (scissorOutOfDate)
	{
		vkCmdSetScissor(cb, 0, 1, &scissor);
		scissorOutOfDate = false;
	}

	if (cullModeOutOfDate && enableDynamicCullMode)
	{
		vkCmdSetCullModeEXT(cb, cullMode);
		cullModeOutOfDate = false;
	}

	if (polygonModeOutOfDate && enableDynamicPolygonMode)
	{
		vkCmdSetPolygonModeEXT(cb, polygonMode);
		polygonModeOutOfDate = false;
	}
}

void VulkanCommandContext::SetViewport(float x, float y, float w, float h)
{
	if (viewportX != x || viewportY != y || viewportW != w || viewportH != h)
	{
		viewportX = x;
		viewportY = y;
		viewportW = w;
		viewportH = h;
		viewportOutOfDate = true;
	}
}

void VulkanCommandContext::SetScissor(int x, int y, int w, int h)
{
	VkRect2D newScissor;
	newScissor.offset.x = std::max<int>(x, 0);
	newScissor.offset.y = std::max<int>(framebufferH - (y + h), 0);
	newScissor.extent.width = glm::clamp(w, 0, ToInt(framebufferW) - x);
	newScissor.extent.height = glm::clamp(h, 0, ToInt(framebufferH) - newScissor.offset.y);

	if (std::memcmp(&newScissor, &scissor, sizeof(VkRect2D)))
	{
		scissor = newScissor;
		scissorOutOfDate = true;
	}
}

void VulkanCommandContext::UpdateDynamicDescriptor(
	uint32_t set, uint32_t binding, VkDescriptorType descriptorType, const VkDescriptorBufferInfo& info)
{
	EG_ASSERT(set < MAX_DESCRIPTOR_SETS);

	auto* infoCopy = m_pushDescriptorInfoAllocator.New<VkDescriptorBufferInfo>(info);

	m_pendingPushDescriptorWrites[set].push_back(VkWriteDescriptorSet{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = descriptorType,
		.pBufferInfo = infoCopy,
	});
}

void VulkanCommandContext::UpdateDynamicDescriptor(
	uint32_t set, uint32_t binding, VkDescriptorType descriptorType, const VkDescriptorImageInfo& info)
{
	EG_ASSERT(set < MAX_DESCRIPTOR_SETS);

	auto* infoCopy = m_pushDescriptorInfoAllocator.New<VkDescriptorImageInfo>(info);

	m_pendingPushDescriptorWrites[set].push_back(VkWriteDescriptorSet{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = descriptorType,
		.pImageInfo = infoCopy,
	});
}

void VulkanCommandContext::FlushDescriptorUpdates()
{
	for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
	{
		if (!m_pendingPushDescriptorWrites[i].empty())
		{
			vkCmdPushDescriptorSetKHR(
				cb, pipeline->bindPoint, pipeline->pipelineLayout, i,
				UnsignedNarrow<uint32_t>(m_pendingPushDescriptorWrites[i].size()),
				m_pendingPushDescriptorWrites[i].data());

			m_pendingPushDescriptorWrites[i].clear();
		}
	}

	m_pushDescriptorInfoAllocator.Reset();
}
} // namespace eg::graphics_api::vk

#include "VulkanCommandContext.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "EGame/Assert.hpp"
#include "EGame/Graphics/Abstraction.hpp"
#include "EGame/Graphics/Vulkan/Common.hpp"
#include "EGame/Utils.hpp"
#include "Pipeline.hpp"

#include <cstring>
#include <vulkan/vulkan_core.h>

namespace eg::graphics_api::vk
{
VulkanCommandContext* VulkanCommandContext::currentImmediate;
std::vector<VulkanCommandContext> VulkanCommandContext::immediateContexts;

static ConcurrentObjectPool<VulkanCommandContext> commandContextPool;

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
	EG_ASSERT(ctx.hasPushDescriptorExtension);
	EG_ASSERT(pipeline && pipeline->descriptorSetBindMode[set] == BindMode::Dynamic);

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
	EG_ASSERT(ctx.hasPushDescriptorExtension);
	EG_ASSERT(pipeline && pipeline->descriptorSetBindMode[set] == BindMode::Dynamic);

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

CommandContextHandle CreateCommandContext(Queue queue)
{
	const VkCommandPoolCreateInfo poolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.queueFamilyIndex = ctx.queueFamily,
	};
	VkCommandPool commandPool;
	CheckRes(vkCreateCommandPool(ctx.device, &poolCreateInfo, nullptr, &commandPool));

	const VkCommandBufferAllocateInfo cbAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkCommandBuffer commandBuffer;
	CheckRes(vkAllocateCommandBuffers(ctx.device, &cbAllocateInfo, &commandBuffer));

	VulkanCommandContext* context = commandContextPool.New();
	context->cb = commandBuffer;
	context->commandPool = commandPool;

	return reinterpret_cast<CommandContextHandle>(context);
}

void DestroyCommandContext(CommandContextHandle context)
{
	EG_ASSERT(context != nullptr);
	VulkanCommandContext& vcc = UnwrapCC(context);
	vkDestroyCommandPool(ctx.device, vcc.commandPool, nullptr);
	commandContextPool.Delete(&vcc);
}

void BeginRecordingCommandContext(CommandContextHandle context, CommandContextBeginFlags flags)
{
	EG_ASSERT(context != nullptr);
	VulkanCommandContext& vcc = UnwrapCC(context);

	CheckRes(vkResetCommandPool(ctx.device, vcc.commandPool, 0));

	VkCommandBufferUsageFlags usageFlags = 0;
	if (HasFlag(flags, CommandContextBeginFlags::OneTimeSubmit))
		usageFlags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (HasFlag(flags, CommandContextBeginFlags::SimultaneousUse))
		usageFlags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = usageFlags,
	};

	CheckRes(vkBeginCommandBuffer(vcc.cb, &beginInfo));
}

void FinishRecordingCommandContext(CommandContextHandle context)
{
	EG_ASSERT(context != nullptr);
	vkEndCommandBuffer(UnwrapCC(context).cb);
}

void SubmitCommandContext(CommandContextHandle context, const CommandContextSubmitArgs& args)
{
	EG_ASSERT(context != nullptr);

	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &UnwrapCC(context).cb,
	};

	vkQueueSubmit(ctx.mainQueue, 1, &submitInfo, reinterpret_cast<VkFence>(args.fence));
}

} // namespace eg::graphics_api::vk

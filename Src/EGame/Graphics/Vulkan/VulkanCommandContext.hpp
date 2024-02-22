#pragma once

#include "../../Alloc/LinearAllocator.hpp"
#include "../Abstraction.hpp"
#include "Common.hpp"

#include <vector>

namespace eg::graphics_api::vk
{
class VulkanCommandContext
{
public:
	static VulkanCommandContext* currentImmediate;
	static std::vector<VulkanCommandContext> immediateContexts;

	ReferencedResourceSet referencedResources;

	VkCommandBuffer cb;

	float viewportX;
	float viewportY;
	float viewportW;
	float viewportH;
	VkRect2D scissor;
	bool viewportOutOfDate;
	bool scissorOutOfDate;

	VkPolygonMode polygonMode;
	bool polygonModeOutOfDate;
	bool enableDynamicPolygonMode;

	VkCullModeFlags cullMode;
	bool cullModeOutOfDate;
	bool enableDynamicCullMode;

	struct AbstractPipeline* pipeline;
	uint32_t framebufferW;
	uint32_t framebufferH;

	void UpdateDynamicDescriptor(
		uint32_t set, uint32_t binding, VkDescriptorType descriptorType, const VkDescriptorBufferInfo& info);
	void UpdateDynamicDescriptor(
		uint32_t set, uint32_t binding, VkDescriptorType descriptorType, const VkDescriptorImageInfo& info);

	void FlushDescriptorUpdates();

	void SetInitialState();

	void FlushDynamicState();

	void SetViewport(float x, float y, float w, float h);
	void SetScissor(int x, int y, int w, int h);

private:
	std::vector<VkWriteDescriptorSet> m_pendingPushDescriptorWrites[MAX_DESCRIPTOR_SETS];
	LinearAllocator m_pushDescriptorInfoAllocator;
};

inline VulkanCommandContext& UnwrapCC(CommandContextHandle handle)
{
	if (handle == nullptr)
		return *VulkanCommandContext::currentImmediate;
	return *reinterpret_cast<VulkanCommandContext*>(handle);
}
} // namespace eg::graphics_api::vk

#include "Buffer.hpp"
#include "Pipeline.hpp"
#include "Texture.hpp"
#include "VulkanCommandContext.hpp"

namespace eg::graphics_api::vk
{
void BindUniformBuffer(
	CommandContextHandle cc, BufferHandle bufferHandle, uint32_t set, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* buffer = UnwrapBuffer(bufferHandle);
	vcc.referencedResources.Add(*buffer);

	buffer->CheckUsageState(BufferUsage::UniformBuffer, "binding as a uniform buffer");

	vcc.UpdateDynamicDescriptor(
		set, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VkDescriptorBufferInfo{
			.buffer = buffer->buffer,
			.offset = offset,
			.range = range.value_or(VK_WHOLE_SIZE),
		});
}

void BindStorageBuffer(
	CommandContextHandle cc, BufferHandle bufferHandle, uint32_t set, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* buffer = UnwrapBuffer(bufferHandle);
	vcc.referencedResources.Add(*buffer);

	if (buffer->autoBarrier && buffer->currentUsage != BufferUsage::StorageBufferRead &&
	    buffer->currentUsage != BufferUsage::StorageBufferWrite &&
	    buffer->currentUsage != BufferUsage::StorageBufferReadWrite)
	{
		EG_PANIC("Buffer not in the correct usage state when binding as a storage buffer, did you forget to call "
		         "UsageHint?");
	}

	vcc.UpdateDynamicDescriptor(
		set, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VkDescriptorBufferInfo{
			.buffer = buffer->buffer,
			.offset = offset,
			.range = range.value_or(VK_WHOLE_SIZE),
		});
}

void BindSampler(CommandContextHandle cc, SamplerHandle sampler, uint32_t set, uint32_t binding)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);

	vcc.UpdateDynamicDescriptor(
		set, binding, VK_DESCRIPTOR_TYPE_SAMPLER,
		VkDescriptorImageInfo{ .sampler = reinterpret_cast<VkSampler>(sampler) });
}

void BindTexture(CommandContextHandle cc, TextureViewHandle textureViewHandle, uint32_t set, uint32_t binding)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	TextureView* view = UnwrapTextureView(textureViewHandle);
	vcc.referencedResources.Add(*view->texture);

	TextureUsage currentUsage = TextureUsage::ShaderSample; // TODO: Support read only depth with manual barriers

	if (view->texture->autoBarrier)
	{
		if (view->texture->currentUsage != TextureUsage::ShaderSample &&
		    view->texture->currentUsage != TextureUsage::DepthStencilReadOnly)
		{
			EG_PANIC("Texture passed to BindTexture not in the correct usage state, did you forget to call UsageHint?");
		}
		currentUsage = view->texture->currentUsage;
	}

	vcc.UpdateDynamicDescriptor(
		set, binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VkDescriptorImageInfo{
			.imageView = view->view,
			.imageLayout = ImageLayoutFromUsage(currentUsage, view->texture->aspectFlags),
		});
}

void BindStorageImage(CommandContextHandle cc, TextureViewHandle textureViewHandle, uint32_t set, uint32_t binding)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	TextureView* view = UnwrapTextureView(textureViewHandle);
	vcc.referencedResources.Add(*view->texture);

	if (view->texture->autoBarrier && view->texture->currentUsage != TextureUsage::ILSRead &&
	    view->texture->currentUsage != TextureUsage::ILSWrite &&
	    view->texture->currentUsage != TextureUsage::ILSReadWrite)
	{
		EG_PANIC(
			"Texture passed to BindStorageImage not in the correct usage state, did you forget to call UsageHint?");
	}

	vcc.UpdateDynamicDescriptor(
		set, binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VkDescriptorImageInfo{
			.sampler = VK_NULL_HANDLE,
			.imageView = view->view,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		});
}
} // namespace eg::graphics_api::vk

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

void BindTexture(
	CommandContextHandle cc, TextureViewHandle textureViewHandle, SamplerHandle samplerHandle, uint32_t set,
	uint32_t binding)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	TextureView* view = UnwrapTextureView(textureViewHandle);
	vcc.referencedResources.Add(*view->texture);

	if (view->texture->autoBarrier && view->texture->currentUsage != TextureUsage::ShaderSample)
	{
		EG_PANIC("Texture passed to BindTexture not in the correct usage state, did you forget to call UsageHint?");
	}

	VkSampler sampler = reinterpret_cast<VkSampler>(samplerHandle);
	EG_ASSERT(sampler != VK_NULL_HANDLE);

	vcc.UpdateDynamicDescriptor(
		set, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VkDescriptorImageInfo{
			.sampler = sampler,
			.imageView = view->view,
			.imageLayout = ImageLayoutFromUsage(eg::TextureUsage::ShaderSample, view->texture->aspectFlags),
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

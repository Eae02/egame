#include "../../Assert.hpp"
#include "Common.hpp"
#include "Translation.hpp"

namespace eg::graphics_api::vk
{
static std::vector<std::pair<SamplerDescription, VkSampler>> samplers;

inline VkSamplerAddressMode TranslateAddressMode(WrapMode mode)
{
	switch (mode)
	{
	case WrapMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case WrapMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case WrapMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	}
	EG_UNREACHABLE
}

void DestroySamplers()
{
	for (const auto& sampler : samplers)
	{
		vkDestroySampler(ctx.device, sampler.second, nullptr);
	}
	samplers.clear();
}

SamplerHandle CreateSampler(const SamplerDescription& description)
{
	const VkSamplerCreateInfo samplerCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = description.magFilter == TextureFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
		.minFilter = description.minFilter == TextureFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
		.mipmapMode = description.mipFilter == TextureFilter::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR
		                                                             : VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = TranslateAddressMode(description.wrapU),
		.addressModeV = TranslateAddressMode(description.wrapV),
		.addressModeW = TranslateAddressMode(description.wrapW),
		.mipLodBias = description.mipLodBias,
		.anisotropyEnable = static_cast<VkBool32>(description.maxAnistropy > 1),
		.maxAnisotropy =
			glm::clamp(static_cast<float>(description.maxAnistropy), 1.0f, ctx.deviceLimits.maxSamplerAnisotropy),
		.compareEnable = static_cast<VkBool32>(description.enableCompare),
		.compareOp = TranslateCompareOp(description.compareOp),
		.minLod = description.minLod,
		.maxLod = description.maxLod,
	};

	VkSampler sampler;
	CheckRes(vkCreateSampler(ctx.device, &samplerCreateInfo, nullptr, &sampler));

	return reinterpret_cast<SamplerHandle>(sampler);
}
} // namespace eg::graphics_api::vk

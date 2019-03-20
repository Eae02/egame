#include "Sampler.hpp"

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
		case WrapMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		}
		EG_UNREACHABLE
	}
	
	inline VkBorderColor TranslateBorderColor(BorderColor color)
	{
		switch (color)
		{
		case BorderColor::F0000: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		case BorderColor::I0000: return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
		case BorderColor::F0001: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		case BorderColor::I0001: return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		case BorderColor::F1111: return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		case BorderColor::I1111: return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
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
	
	VkSampler GetSampler(const SamplerDescription& description)
	{
		for (const auto& sampler : samplers)
		{
			if (sampler.first == description)
				return sampler.second;
		}
		
		VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerCreateInfo.addressModeU = TranslateAddressMode(description.wrapU);
		samplerCreateInfo.addressModeV = TranslateAddressMode(description.wrapV);
		samplerCreateInfo.addressModeW = TranslateAddressMode(description.wrapW);
		samplerCreateInfo.mipLodBias = description.mipLodBias;
		samplerCreateInfo.anisotropyEnable = (VkBool32)(description.maxAnistropy > 1);
		samplerCreateInfo.maxAnisotropy = glm::clamp((float)description.maxAnistropy, 1.0f, ctx.deviceLimits.maxSamplerAnisotropy);
		samplerCreateInfo.borderColor = TranslateBorderColor(description.borderColor);
		samplerCreateInfo.minLod = -1000;
		samplerCreateInfo.maxLod = 1000;
		samplerCreateInfo.minFilter = description.minFilter == TextureFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		samplerCreateInfo.magFilter = description.magFilter == TextureFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = description.mipFilter == TextureFilter::Linear ?
			VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.compareEnable = (VkBool32)description.enableCompare;
		samplerCreateInfo.compareOp = TranslateCompareOp(description.compareOp);
		
		VkSampler sampler;
		CheckRes(vkCreateSampler(ctx.device, &samplerCreateInfo, nullptr, &sampler));
		samplers.emplace_back(description, sampler);
		
		return sampler;
	}
	
	SamplerHandle CreateSampler(const SamplerDescription& description)
	{
		return reinterpret_cast<SamplerHandle>(GetSampler(description));
	}
	
	void DestroySampler(SamplerHandle sampler) { }
}

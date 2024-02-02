#ifndef EG_NO_VULKAN
#include "ShaderModule.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"

#include <spirv_cross.hpp>

namespace eg::graphics_api::vk
{
static ConcurrentObjectPool<ShaderModule> shaderModulesPool;

void ShaderModule::UnRef()
{
	if (ref-- == 1)
	{
		vkDestroyShaderModule(ctx.device, module, nullptr);
		shaderModulesPool.Delete(this);
	}
}

static const std::array<VkShaderStageFlags, 6> ShaderStageFlags = { VK_SHADER_STAGE_VERTEX_BIT,
	                                                                VK_SHADER_STAGE_FRAGMENT_BIT,
	                                                                VK_SHADER_STAGE_GEOMETRY_BIT,
	                                                                VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
	                                                                VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
	                                                                VK_SHADER_STAGE_COMPUTE_BIT };

ShaderModuleHandle CreateShaderModule(ShaderStage stage, std::span<const char> code)
{
	ShaderModule* module = shaderModulesPool.New();
	module->ref = 1;

	// Creates the shader module
	VkShaderModuleCreateInfo moduleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	moduleCreateInfo.codeSize = code.size();
	moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	CheckRes(vkCreateShaderModule(ctx.device, &moduleCreateInfo, nullptr, &module->module));

	VkShaderStageFlags stageFlags = ShaderStageFlags.at(static_cast<int>(stage));

	spirv_cross::Compiler spvCrossCompiler(moduleCreateInfo.pCode, moduleCreateInfo.codeSize / 4);

	// Processes shader resources
	auto ProcessResources = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& resources, VkDescriptorType type)
	{
		for (const spirv_cross::Resource& resource : resources)
		{
			const uint32_t set = spvCrossCompiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			const uint32_t binding = spvCrossCompiler.get_decoration(resource.id, spv::DecorationBinding);
			const uint32_t count = 1;

			auto it = std::find_if(
				module->bindings[set].begin(), module->bindings[set].end(),
				[&](const VkDescriptorSetLayoutBinding& b) { return b.binding == binding; });

			if (it != module->bindings[set].end())
			{
				if (it->descriptorType != type)
					EG_PANIC("Descriptor type mismatch for binding " << binding << " in set " << set);
				if (it->descriptorCount != count)
					EG_PANIC("Descriptor count mismatch for binding " << binding << " in set " << set);
				it->stageFlags |= stageFlags;
			}
			else
			{
				VkDescriptorSetLayoutBinding& bindingRef = module->bindings[set].emplace_back();
				bindingRef.binding = binding;
				bindingRef.stageFlags = stageFlags;
				bindingRef.descriptorType = type;
				bindingRef.descriptorCount = count;
				bindingRef.pImmutableSamplers = nullptr;
			}
		}
	};

	const spirv_cross::ShaderResources& resources = spvCrossCompiler.get_shader_resources();
	ProcessResources(resources.uniform_buffers, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	ProcessResources(resources.storage_buffers, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	ProcessResources(resources.sampled_images, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	ProcessResources(resources.separate_images, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	ProcessResources(resources.separate_samplers, VK_DESCRIPTOR_TYPE_SAMPLER);
	ProcessResources(resources.storage_images, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	module->pushConstantBytes = 0;
	for (const spirv_cross::Resource& pcBlock : resources.push_constant_buffers)
	{
		for (const spirv_cross::BufferRange& range : spvCrossCompiler.get_active_buffer_ranges(pcBlock.id))
		{
			module->pushConstantBytes =
				std::max(module->pushConstantBytes, UnsignedNarrow<uint32_t>(range.offset + range.range));
		}
	}

	return reinterpret_cast<ShaderModuleHandle>(module);
}

void DestroyShaderModule(ShaderModuleHandle handle)
{
	UnwrapShaderModule(handle)->UnRef();
}
} // namespace eg::graphics_api::vk
#endif

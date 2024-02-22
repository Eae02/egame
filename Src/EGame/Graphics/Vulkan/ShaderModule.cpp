#ifndef EG_NO_VULKAN
#include "ShaderModule.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "../SpirvCrossUtils.hpp"

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

static const std::array<VkShaderStageFlags, 6> shaderStageFlags = {
	VK_SHADER_STAGE_VERTEX_BIT,
	VK_SHADER_STAGE_FRAGMENT_BIT,
	VK_SHADER_STAGE_GEOMETRY_BIT,
	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
	VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
	VK_SHADER_STAGE_COMPUTE_BIT,
};

ShaderModuleHandle CreateShaderModule(ShaderStage stage, const spirv_cross::ParsedIR& parsedIR)
{
	ShaderModule* module = shaderModulesPool.New();
	module->ref = 1;

	// Creates the shader module
	VkShaderModuleCreateInfo moduleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	moduleCreateInfo.codeSize = parsedIR.spirv.size() * sizeof(uint32_t);
	moduleCreateInfo.pCode = parsedIR.spirv.data();
	CheckRes(vkCreateShaderModule(ctx.device, &moduleCreateInfo, nullptr, &module->module));

	spirv_cross::Compiler spvCrossCompiler(parsedIR);

	const spirv_cross::ShaderResources& resources = spvCrossCompiler.get_shader_resources();

	module->bindings.AppendFromReflectionInfo(stage, spvCrossCompiler, resources);
	module->pushConstantBytes = GetPushConstantBytes(spvCrossCompiler, &resources);

	return reinterpret_cast<ShaderModuleHandle>(module);
}

void DestroyShaderModule(ShaderModuleHandle handle)
{
	UnwrapShaderModule(handle)->UnRef();
}
} // namespace eg::graphics_api::vk
#endif

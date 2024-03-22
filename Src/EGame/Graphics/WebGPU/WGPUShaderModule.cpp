#include "WGPUShaderModule.hpp"
#include "../Abstraction.hpp"
#include "WGPUTint.hpp"

#include <spirv-tools/optimizer.hpp>
#include <spirv_cross.hpp>

namespace eg::graphics_api::webgpu
{
WGPUShaderModule CreateShaderModuleFromSpirV(std::span<const uint32_t> spirv, const char* label)
{
	WGPUShaderModuleWGSLDescriptor wgslDescriptor;
	WGPUShaderModuleSPIRVDescriptor spirvDescriptor;

	const void* languageDescriptor = nullptr;

	std::optional<std::string> wgsl;

	if (UseWGSL())
	{
		wgsl = GenerateShaderWGSL({ reinterpret_cast<const char*>(spirv.data()), spirv.size_bytes() }, label);
		if (!wgsl.has_value())
			return nullptr;
		wgslDescriptor = {
			.chain = { .sType = WGPUSType_ShaderModuleWGSLDescriptor },
			.code = wgsl->c_str(),
		};
		languageDescriptor = &wgslDescriptor;
	}
	else
	{
		spirvDescriptor = {
			.chain = { .sType = WGPUSType_ShaderModuleSPIRVDescriptor },
			.codeSize = UnsignedNarrow<uint32_t>(spirv.size()),
			.code = spirv.data(),
		};
		languageDescriptor = &spirvDescriptor;
	}

	const WGPUShaderModuleDescriptor shaderModuleDesc = {
		.nextInChain = static_cast<const WGPUChainedStruct*>(languageDescriptor),
		.label = label,
	};

	return wgpuDeviceCreateShaderModule(wgpuctx.device, &shaderModuleDesc);
}

std::unique_ptr<WGPUShaderModuleImpl, ShaderModuleOptDeleter> ShaderModule::GetSpecializedShaderModule(
	std::span<const SpecializationConstantEntry> specConstantEntries) const
{
	if (shaderModule != nullptr)
		return { shaderModule, ShaderModuleOptDeleter{ .shouldRelease = false } };

	EG_ASSERT(!spirvForLateCompile.empty());

	std::unordered_map<uint32_t, std::string> specConstantIdToValue;
	for (const SpecializationConstantEntry& entry : specConstantEntries)
	{
		specConstantIdToValue.emplace(
			entry.constantID, std::visit([&](auto v) { return std::to_string(v); }, entry.value));
	}

	specConstantIdToValue[500] = "3"; // Identifier for webgpu

	spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_1);
	optimizer.RegisterPass(spvtools::CreateSetSpecConstantDefaultValuePass(specConstantIdToValue));
	optimizer.RegisterPass(spvtools::CreateFreezeSpecConstantValuePass());
	optimizer.RegisterPass(spvtools::CreateFoldSpecConstantOpAndCompositePass());

	std::vector<uint32_t> specializedSpirV;
	if (!optimizer.Run(spirvForLateCompile.data(), spirvForLateCompile.size(), &specializedSpirV))
	{
		EG_PANIC("Failed to specialize spirv for WGSL conversion");
	}

	return { CreateShaderModuleFromSpirV(specializedSpirV, label.empty() ? nullptr : label.c_str()),
		     ShaderModuleOptDeleter{ .shouldRelease = true } };
}

void ShaderModuleOptDeleter::operator()(WGPUShaderModule shaderModule) const
{
	if (shouldRelease)
		wgpuShaderModuleRelease(shaderModule);
}

ShaderModuleHandle CreateShaderModule(ShaderStage stage, const spirv_cross::ParsedIR& parsedIR, const char* label)
{
	std::span<const uint32_t> spirv(parsedIR.spirv);

	ShaderModule* module = new ShaderModule;

	spirv_cross::Compiler spvCrossCompiler(parsedIR);
	const spirv_cross::ShaderResources& resources = spvCrossCompiler.get_shader_resources();
	module->bindings.AppendFromReflectionInfo(stage, spvCrossCompiler, resources);

	if (label != nullptr)
		module->label = label;

	if (spvCrossCompiler.get_specialization_constants().empty())
	{
		module->shaderModule = CreateShaderModuleFromSpirV(spirv, label);
	}
	else
	{
		module->spirvForLateCompile.assign(spirv.begin(), spirv.end());
	}

	return reinterpret_cast<ShaderModuleHandle>(module);
}

void DestroyShaderModule(ShaderModuleHandle handle)
{
	ShaderModule* module = reinterpret_cast<ShaderModule*>(handle);
	if (module->shaderModule != nullptr)
		wgpuShaderModuleRelease(module->shaderModule);
	delete module;
}
} // namespace eg::graphics_api::webgpu

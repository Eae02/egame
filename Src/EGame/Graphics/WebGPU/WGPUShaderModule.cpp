#include "WGPUShaderModule.hpp"
#include "../../String.hpp"
#include "../Abstraction.hpp"

#include <src/tint/lang/core/ir/module.h>
#include <src/tint/lang/spirv/reader/ast_lower/pass_workgroup_id_as_argument.h>
#include <src/tint/lang/wgsl/ast/transform/push_constant_helper.h>
#include <src/tint/lang/wgsl/writer/ir_to_program/ir_to_program.h>
#include <string>
#include <tint/tint.h>

#include <spirv_cross.hpp>

namespace eg::graphics_api::webgpu
{
void ShaderModule::InitializeTint()
{
	tint::Initialize();
	tint::SetInternalCompilerErrorReporter([](const tint::InternalCompilerError& e)
	                                       { std::cerr << "Internal tint error: " << e.Message() << std::endl; });
}

std::optional<std::string> GenerateShaderWGSL(std::span<const char> spirv, const char* label)
{
	std::string labelWithParenthesis;
	if (label != nullptr)
		labelWithParenthesis = Concat({ " (", label, ")" });

	std::vector<uint32_t> spirvVectorCopy(
		reinterpret_cast<const uint32_t*>(spirv.data()),
		reinterpret_cast<const uint32_t*>(spirv.data()) + spirv.size() / sizeof(uint32_t));

	tint::spirv::reader::PassWorkgroupIdAsArgument x;

	tint::spirv::reader::Options spirvOptions;
	auto program = tint::spirv::reader::Read(spirvVectorCopy, spirvOptions);
	if (!program.IsValid() || program.Diagnostics().ContainsErrors())
	{
		std::ostringstream message;
		message << "Failed to convert to WGSL" << labelWithParenthesis << ": " << program.Diagnostics();
		eg::Log(eg::LogLevel::Error, "sh", "{0}", message.str());
		return std::nullopt;
	}

	tint::wgsl::writer::Options gen_options;
	auto result = tint::wgsl::writer::Generate(program, gen_options);
	if (result != tint::Success)
	{
		std::ostringstream message;
		message << "Failed to write WGSL" << labelWithParenthesis << ": " << result.Failure();
		eg::Log(eg::LogLevel::Error, "sh", "{0}", message.str());
		return std::nullopt;
	}

	return result->wgsl;
}

ShaderModuleHandle CreateShaderModule(ShaderStage stage, const spirv_cross::ParsedIR& parsedIR, const char* label)
{
	std::span<const uint32_t> spirv(parsedIR.spirv);
	std::optional<std::string> wgsl =
		GenerateShaderWGSL({ reinterpret_cast<const char*>(spirv.data()), spirv.size_bytes() }, label);

	if (!wgsl.has_value())
		return nullptr;

	const WGPUShaderModuleWGSLDescriptor wgslDescriptor = { .code = wgsl->c_str() };
	const WGPUShaderModuleDescriptor shaderModuleDesc = {
		.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&wgslDescriptor),
		.label = label,
	};

	ShaderModule* module = new ShaderModule;
	module->shaderModule = wgpuDeviceCreateShaderModule(wgpuctx.device, &shaderModuleDesc);
	
	spirv_cross::Compiler spvCrossCompiler(parsedIR);
	const spirv_cross::ShaderResources& resources = spvCrossCompiler.get_shader_resources();
	module->bindings.AppendFromReflectionInfo(stage, spvCrossCompiler, resources);

	return reinterpret_cast<ShaderModuleHandle>(module);
}

void DestroyShaderModule(ShaderModuleHandle handle)
{
	delete reinterpret_cast<ShaderModule*>(handle);
}
} // namespace eg::graphics_api::webgpu

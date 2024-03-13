#ifdef EG_WGSL

#include <src/tint/lang/spirv/reader/ast_lower/pass_workgroup_id_as_argument.h>
#include <src/tint/lang/wgsl/ast/transform/push_constant_helper.h>
#include <string>

#include "../EGame/Log.hpp"
#include <span>
#include <src/tint/lang/core/ir/module.h>
#include <src/tint/lang/wgsl/writer/ir_to_program/ir_to_program.h>
#include <tint/tint.h>

#include <iostream>

namespace eg::asset_gen
{
static bool hasInitializedTint;

std::optional<std::string> GenerateShaderWGSL(std::span<const char> spirv)
{
	if (!hasInitializedTint)
	{
		hasInitializedTint = true;
		tint::Initialize();
		tint::SetInternalCompilerErrorReporter([](const tint::InternalCompilerError& e)
		                                       { std::cerr << "Internal tint error: " << e.Message() << std::endl; });
	}

	std::vector<uint32_t> spirvVectorCopy(
		reinterpret_cast<const uint32_t*>(spirv.data()),
		reinterpret_cast<const uint32_t*>(spirv.data()) + spirv.size() / sizeof(uint32_t));

	tint::spirv::reader::PassWorkgroupIdAsArgument x;

	tint::spirv::reader::Options spirvOptions;
	auto program = tint::spirv::reader::Read(spirvVectorCopy, spirvOptions);
	if (!program.IsValid() || program.Diagnostics().ContainsErrors())
	{
		std::ostringstream message;
		message << "Failed to convert to WGSL: " << program.Diagnostics();
		eg::Log(eg::LogLevel::Error, "sh", "{0}", message.str());
		return std::nullopt;
	}

	tint::wgsl::writer::Options gen_options;
	auto result = tint::wgsl::writer::Generate(program, gen_options);
	if (result != tint::Success)
	{
		std::ostringstream message;
		message << "Failed to write WGSL: " << result.Failure();
		eg::Log(eg::LogLevel::Error, "sh", "{0}", message.str());
		return std::nullopt;
	}

	return result->wgsl;
}
} // namespace eg::asset_gen

#endif

#include "WGPUTint.hpp"
#ifdef EG_WEBGPU_TINT

#include "../../String.hpp"

#include <src/tint/lang/core/ir/module.h>
#include <src/tint/lang/spirv/reader/ast_lower/pass_workgroup_id_as_argument.h>
#include <src/tint/lang/wgsl/ast/transform/push_constant_helper.h>
#include <src/tint/lang/wgsl/writer/ir_to_program/ir_to_program.h>
#include <tint/tint.h>

namespace eg::graphics_api::webgpu
{
static void TintErrorReporter(const tint::InternalCompilerError& e)
{
	std::cerr << "Internal tint error: " << e.Message() << std::endl;
}

static bool useWGSL;

void InitializeTint()
{
	tint::Initialize();
	tint::SetInternalCompilerErrorReporter(TintErrorReporter);
	useWGSL = true;
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

bool UseWGSL()
{
	return useWGSL;
}
} // namespace eg::graphics_api::webgpu

#else

namespace eg::graphics_api::webgpu
{
void InitializeTint() {}

std::optional<std::string> GenerateShaderWGSL(std::span<const char> spirv, const char* label)
{
	return std::nullopt;
}

bool UseWGSL()
{
	return false;
}
} // namespace eg::graphics_api::webgpu

#endif

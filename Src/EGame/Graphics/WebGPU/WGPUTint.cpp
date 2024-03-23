#include "WGPUTint.hpp"
#ifdef EG_ENABLE_TINT

#include "../../Log.hpp"
#include "../../String.hpp"

#include <src/tint/lang/core/ir/module.h>
#include <src/tint/lang/spirv/reader/ast_lower/pass_workgroup_id_as_argument.h>
#include <src/tint/lang/wgsl/ast/transform/push_constant_helper.h>
#include <src/tint/lang/wgsl/writer/ir_to_program/ir_to_program.h>
#include <tint/tint.h>

#include <iostream>

namespace eg::graphics_api::webgpu
{
static void TintErrorReporter(const tint::InternalCompilerError& e)
{
	eg::Log(eg::LogLevel::Error, "webgpu", "Internal tint error: {0}", e.Message());
}

static bool useWGSL;
static bool dumpWGSL;

void InitializeTint()
{
	tint::Initialize();
	tint::SetInternalCompilerErrorReporter(TintErrorReporter);
	useWGSL = true;

	if (const char* useWGSLEnv = std::getenv("EG_USE_WGSL"))
	{
		if (std::string_view(useWGSLEnv) == "0")
			useWGSL = false;
	}

	if (const char* useWGSLEnv = std::getenv("EG_DUMP_WGSL"))
	{
		if (std::string_view(useWGSLEnv) == "1")
			dumpWGSL = true;
	}
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
		eg::Log(eg::LogLevel::Error, "webgpu", "{0}", message.str());
		return std::nullopt;
	}

	tint::wgsl::writer::Options gen_options;
	auto result = tint::wgsl::writer::Generate(program, gen_options);
	if (result != tint::Success)
	{
		std::ostringstream message;
		message << "Failed to write WGSL" << labelWithParenthesis << ": " << result.Failure();
		eg::Log(eg::LogLevel::Error, "webgpu", "{0}", message.str());
		return std::nullopt;
	}

	if (dumpWGSL)
	{
		std::cerr << "-- WGSL Dump ";
		if (label)
			std::cerr << "[" << label << "]";
		std::cerr << " --\n";
		IterateStringParts(result->wgsl, '\n', [&](std::string_view line) { std::cerr << " |   " << line << "\n"; });
		std::cerr << "---------------\n\n";
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

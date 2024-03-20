#pragma once

#include "Abstraction.hpp"
#include "Graphics.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include <array>

namespace eg
{
EG_API void SetSpecializationConstants(const struct ShaderStageInfo& stageInfo, spirv_cross::Compiler& compiler);

EG_API uint32_t
GetPushConstantBytes(const spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources* shaderResources);

EG_API std::unique_ptr<spirv_cross::ParsedIR, SpirvCrossParsedIRDeleter> ParseSpirV(std::span<const uint32_t> spirv);

EG_API std::optional<glm::uvec3> GetWorkGroupSizeFromSpirV(
	const spirv_cross::Compiler& compiler, std::span<const SpecializationConstantEntry> specConstants);

struct EG_API DescriptorSetBindings
{
	enum class AppendResult
	{
		Ok,
		TypeMismatch,
	};

	static void AssertAppendOk(AppendResult result, uint32_t set, uint32_t binding);

	std::array<std::vector<struct DescriptorSetBinding>, MAX_DESCRIPTOR_SETS> sets;

	DescriptorSetBindings() = default;

	void SortByBinding();

	[[nodiscard]] AppendResult Append(uint32_t set, const DescriptorSetBinding& binding);

	void AppendFromReflectionInfo(
		ShaderStage stage, const spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& shaderResources);

	void AppendFrom(const DescriptorSetBindings& other);
};
} // namespace eg

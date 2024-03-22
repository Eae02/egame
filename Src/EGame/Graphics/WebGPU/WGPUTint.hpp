#pragma once

#include <optional>
#include <span>
#include <string>

namespace eg::graphics_api::webgpu
{
void InitializeTint();

std::optional<std::string> GenerateShaderWGSL(std::span<const char> spirv, const char* label);

bool UseWGSL();
} // namespace eg::graphics_api::webgpu

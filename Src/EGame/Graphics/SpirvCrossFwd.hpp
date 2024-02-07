#pragma once

#include "../API.hpp"

namespace spirv_cross
{
class Compiler;
class ShaderResources;
class ParsedIR;
} // namespace spirv_cross

namespace eg
{
struct EG_API SpirvCrossParsedIRDeleter
{
	void operator()(spirv_cross::ParsedIR* parsedIR) const;
};
} // namespace eg

#pragma once

#include "../API.hpp"
#include "AbstractionHL.hpp"

namespace eg
{
namespace detail
{
void DestroyFullscreenShaders();
}

enum class FullscreenShaderTexCoordMode
{
	NoOutput,
	NotFlipped,
	Flipped,
	FlippedIfOpenGL
};

EG_API const ShaderModule& GetFullscreenShader(
	FullscreenShaderTexCoordMode texCoordMode = FullscreenShaderTexCoordMode::FlippedIfOpenGL);
} // namespace eg

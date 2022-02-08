#pragma once

#include "../API.hpp"
#include "Abstraction.hpp"

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
	
	EG_API ShaderModuleHandle GetFullscreenShader(FullscreenShaderTexCoordMode texCoordMode = FullscreenShaderTexCoordMode::FlippedIfOpenGL);
}

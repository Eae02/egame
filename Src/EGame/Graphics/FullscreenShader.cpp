#include "FullscreenShader.hpp"
#include "../../Shaders/Build/Fullscreen_TCFlip.vs.h"
#include "../../Shaders/Build/Fullscreen_TCNoFlip.vs.h"
#include "../../Shaders/Build/Fullscreen_TCNone.vs.h"
#include "../Assert.hpp"

#include <span>

namespace eg
{
static std::array<std::unique_ptr<ShaderModule>, 3> fullScreenShaders;

const ShaderModule& GetFullscreenShader(FullscreenShaderTexCoordMode texCoordMode)
{
	if (texCoordMode == FullscreenShaderTexCoordMode::FlippedIfOpenGL)
	{
		if (CurrentGraphicsAPI() == GraphicsAPI::OpenGL)
			texCoordMode = FullscreenShaderTexCoordMode::Flipped;
		else
			texCoordMode = FullscreenShaderTexCoordMode::NotFlipped;
	}

	if (fullScreenShaders[static_cast<int>(texCoordMode)] == nullptr)
	{
		std::span<const char> shaderCode;
		switch (texCoordMode)
		{
		case FullscreenShaderTexCoordMode::NoOutput:
			shaderCode = { reinterpret_cast<const char*>(Fullscreen_TCNone_vs_glsl),
				           sizeof(Fullscreen_TCNone_vs_glsl) };
			break;
		case FullscreenShaderTexCoordMode::NotFlipped:
			shaderCode = { reinterpret_cast<const char*>(Fullscreen_TCNoFlip_vs_glsl),
				           sizeof(Fullscreen_TCNoFlip_vs_glsl) };
			break;
		case FullscreenShaderTexCoordMode::Flipped:
			shaderCode = { reinterpret_cast<const char*>(Fullscreen_TCFlip_vs_glsl),
				           sizeof(Fullscreen_TCFlip_vs_glsl) };
			break;
		case FullscreenShaderTexCoordMode::FlippedIfOpenGL: EG_UNREACHABLE
		}

		fullScreenShaders[static_cast<int>(texCoordMode)] =
			std::make_unique<ShaderModule>(eg::ShaderStage::Vertex, shaderCode);
	}

	return *fullScreenShaders[static_cast<int>(texCoordMode)];
}

void detail::DestroyFullscreenShaders()
{
	fullScreenShaders = {};
}
} // namespace eg

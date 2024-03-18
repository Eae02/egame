#pragma once

#include <imgui.h>

#include "../EGame/Graphics/AbstractionHL.hpp"

namespace eg::imgui
{
struct InitializeArgs
{
	bool enableImGuiIni = true;
	const char* fontPath = nullptr;
	int fontSize = 14;
};

void Initialize(const InitializeArgs& args);
void Uninitialize(); // Called automatically on shutdown

static_assert(sizeof(ImTextureID) == sizeof(DescriptorSetHandle));

inline ImTextureID MakeImTextureID(const eg::Texture& texture)
{
	return reinterpret_cast<ImTextureID>(texture.GetFragmentShaderSampleDescriptorSet().handle);
}
inline ImTextureID MakeImTextureID(eg::DescriptorSetRef descriptorSet)
{
	return reinterpret_cast<ImTextureID>(descriptorSet.handle);
}
} // namespace eg::imgui

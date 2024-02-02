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

inline ImTextureID MakeImTextureID(TextureViewHandle viewHandle)
{
	static_assert(sizeof(ImTextureID) == sizeof(TextureViewHandle));
	return reinterpret_cast<ImTextureID>(viewHandle);
}
} // namespace eg::imgui

#pragma once

#include <span>

#include "../EGame/Assets/EAPFile.hpp"

void WriteListOutput(std::span<const eg::EAPAsset> assets);
void WriteInfoOutput(std::span<const eg::EAPAsset> assets);

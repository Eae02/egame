#pragma once

#include "../API.hpp"
#include "AssetFormat.hpp"

namespace eg
{
EG_API extern const AssetFormat DefaultGeneratorFormat;

namespace detail
{
void RegisterDefaultAssetGenerator();
}
} // namespace eg

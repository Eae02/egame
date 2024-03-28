#pragma once

namespace eg
{
class AssetManager;

namespace detail
{
extern const AssetManager* commandsAssetManager;

void RegisterConsoleCommands();
} // namespace detail
} // namespace eg

#include "SpriteFontLoader.hpp"
#include "../Graphics/SpriteFont.hpp"
#include "../IOUtils.hpp"
#include "AssetLoad.hpp"

namespace eg
{
const AssetFormat SpriteFontAssetFormat{ "EG::SpriteFont", 0 };

bool SpriteFontLoader(const AssetLoadContext& loadContext)
{
	MemoryReader reader(loadContext.Data());
	loadContext.CreateResult<SpriteFont>(FontAtlas::Deserialize(reader), loadContext.GetGraphicsLoadContext());
	return true;
}
} // namespace eg

#include "SpriteFontLoader.hpp"
#include "AssetLoad.hpp"
#include "../IOUtils.hpp"
#include "../Graphics/SpriteFont.hpp"

namespace eg
{
	const AssetFormat SpriteFontAssetFormat { "EG::SpriteFont", 0 };
	
	bool SpriteFontLoader(const AssetLoadContext& loadContext)
	{
		MemoryStreambuf memStreambuf(loadContext.Data());
		std::istream istream(&memStreambuf);
		loadContext.CreateResult<SpriteFont>(FontAtlas::Deserialize(istream));
		return true;
	}
}

#include "../EGame/API.hpp"

namespace eg::asset_gen
{
	void RegisterShaderGenerator();
	void RegisterTexture2DArrayGenerator();
}

EG_C_EXPORT void Init()
{
	eg::asset_gen::RegisterShaderGenerator();
	eg::asset_gen::RegisterTexture2DArrayGenerator();
}

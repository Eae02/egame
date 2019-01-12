#include "../EGame/API.hpp"

namespace eg::asset_gen
{
	void RegisterShaderGenerator();
}

EG_C_EXPORT void Init()
{
	eg::asset_gen::RegisterShaderGenerator();
}

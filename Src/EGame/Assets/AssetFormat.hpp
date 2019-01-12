#pragma once

#include "../API.hpp"
#include "../Utils.hpp"

namespace eg
{
	struct AssetFormat
	{
		uint32_t nameHash;
		uint32_t version;
		
		AssetFormat() = default;
		AssetFormat(CTStringHash name, uint32_t _version)
			: nameHash(name.hash), version(_version) { }
	};
	
	EG_API extern const AssetFormat DefaultGeneratorFormat;
}

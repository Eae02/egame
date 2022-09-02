#pragma once

#include "../API.hpp"
#include "../Hash.hpp"

namespace eg
{
	struct AssetFormat
	{
		uint32_t nameHash;
		uint32_t version;
		
		AssetFormat() = default;
		AssetFormat(CTStringHash name, uint32_t _version)
			: nameHash(name.hash), version(_version) { }
		
		bool operator==(const AssetFormat& other) const
		{
			return nameHash == other.nameHash && version == other.version;
		}
		bool operator!=(const AssetFormat& other) const
		{
			return !operator==(other);
		}
	};
}

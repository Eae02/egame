#pragma once

#include "../Utils.hpp"

namespace eg
{
#pragma pack(push, 1)
	struct StdVertex
	{
		static constexpr CTStringHash Name = "EG::StdVertex";
		
		float position[3];
		float texCoord[2];
		int8_t normal[4];
		int8_t tangent[4];
		uint8_t color[4];
	};
#pragma pack(pop)
}

#pragma once

#include "../Utils.hpp"
#include "../Color.hpp"

namespace eg
{
	extern const eg::ColorLin loadingBackgroundColor;
	
	std::unique_ptr<uint8_t, FreeDel> GetLoadingImageData(int& width, int& height);
}

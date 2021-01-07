#include "LoadingScreen.hpp"
#include "ImageLoader.hpp"
#include "../IOUtils.hpp"
#include "../../Assets/Loading.png.h"
#include "AbstractionHL.hpp"

namespace eg
{
	const eg::ColorLin loadingBackgroundColor(eg::ColorSRGB(0.063f, 0.192f, 0.29f, 1));
	
	std::unique_ptr<uint8_t, FreeDel> GetLoadingImageData(int& width, int& height)
	{
		MemoryStreambuf imageDataBuf(
			reinterpret_cast<const char*>(Loading_png),
			reinterpret_cast<const char*>(Loading_png + Loading_png_len)
		);
		std::istream stream(&imageDataBuf);
		ImageLoader imageLoader(stream);
		auto data = imageLoader.Load(4);
		width = imageLoader.Width();
		height = imageLoader.Height();
		return data;
	}
}

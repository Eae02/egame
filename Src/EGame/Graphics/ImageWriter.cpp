#include "ImageWriter.hpp"

#include <stb_image_write.h>

namespace eg
{
static void STBIWrite(void* user, void* data, int size)
{
	auto* stream = reinterpret_cast<std::ostream*>(user);
	stream->write(static_cast<char*>(data), size);
}

bool WriteImageToStream(
	std::ostream& stream, WriteImageFormat format, uint32_t width, uint32_t height, uint32_t components,
	std::span<const char> data, int jpgQuality)
{
	switch (format)
	{
	case WriteImageFormat::PNG:
		return stbi_write_png_to_func(STBIWrite, &stream, width, height, components, data.data(), 0) != 0;
	case WriteImageFormat::JPG:
		return stbi_write_jpg_to_func(STBIWrite, &stream, width, height, components, data.data(), jpgQuality) != 0;
	case WriteImageFormat::TGA:
		return stbi_write_tga_to_func(STBIWrite, &stream, width, height, components, data.data()) != 0;
	case WriteImageFormat::BMP:
		return stbi_write_bmp_to_func(STBIWrite, &stream, width, height, components, data.data()) != 0;
	}
	return false;
}
} // namespace eg

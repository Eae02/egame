#pragma once

#include "AbstractionHL.hpp"

#include <ostream>
#include <span>

namespace eg
{
enum class WriteImageFormat
{
	PNG,
	JPG,
	TGA,
	BMP
};

EG_API bool WriteImageToStream(
	std::ostream& stream, WriteImageFormat format, uint32_t width, uint32_t height, uint32_t components,
	std::span<const char> data, int jpgQuality = 80);
} // namespace eg

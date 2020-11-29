#pragma once

#include "../Span.hpp"
#include "AbstractionHL.hpp"

#include <ostream>

namespace eg
{
	enum class WriteImageFormat
	{
		PNG,
		JPG,
		TGA,
		BMP
	};
	
	EG_API bool WriteImageToStream(std::ostream& stream, WriteImageFormat format,
		uint32_t width, uint32_t height, uint32_t components, Span<const char> data,
		int jpgQuality = 80);
}

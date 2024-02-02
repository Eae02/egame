#pragma once

#include "API.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string_view>
#include <vector>

namespace eg
{
EG_API bool ReadCompressedSection(
	std::istream& input, void* output, size_t outputSize, uint64_t* compressedSizeOut = nullptr);
EG_API void WriteCompressedSection(std::ostream& output, const void* data, size_t dataSize);

EG_API std::vector<char> Compress(const void* data, size_t dataSize);
EG_API bool Decompress(const void* input, size_t inputSize, void* output, size_t outputSize);

EG_API std::vector<char> Base64Decode(std::string_view in);
} // namespace eg

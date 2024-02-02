#include "Hash.hpp"

namespace eg
{
uint64_t HashFNV1a64(std::string_view s)
{
	constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
	constexpr uint64_t FNV_PRIME = 1099511628211ull;

	uint64_t h = FNV_OFFSET_BASIS;
	for (char c : s)
	{
		h ^= static_cast<uint8_t>(c);
		h *= FNV_PRIME;
	}
	return h;
}

uint32_t HashFNV1a32(std::string_view s)
{
	constexpr uint32_t FNV_OFFSET_BASIS = 2166136261;
	constexpr uint32_t FNV_PRIME = 16777619;

	uint32_t h = FNV_OFFSET_BASIS;
	for (char c : s)
	{
		h ^= static_cast<uint8_t>(c);
		h *= FNV_PRIME;
	}
	return h;
}
} // namespace eg

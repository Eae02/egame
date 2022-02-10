#pragma once

#include "API.hpp"

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace eg
{
	template <class T>
	inline void HashAppend(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= (size_t)hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
	}
	
	template <typename T>
	struct MemberFunctionHash
	{
		size_t operator()(const T& t) const { return t.Hash(); }
	};
	
	EG_API uint64_t HashFNV1a64(std::string_view s);
	EG_API uint32_t HashFNV1a32(std::string_view s);
	
	class CTStringHash
	{
	public:
		constexpr CTStringHash(uint32_t _hash = 0) noexcept
			: hash(_hash) { }
		
		constexpr CTStringHash(const char* const text) noexcept
			: hash(CalcHash(text, SIZE_MAX, FNV_OFFSET_BASIS)) { }
		
		constexpr CTStringHash(const std::string_view text) noexcept
			: hash(CalcHash(text.data(), text.size(), FNV_OFFSET_BASIS)) { }
		
		constexpr CTStringHash Append(CTStringHash other) const noexcept
		{
			return CTStringHash(other.hash + 0x9e3779b9 + (hash << 6) + (hash >> 2));
		}
		
		uint32_t hash;
		
	private:
		static constexpr uint32_t FNV_OFFSET_BASIS = 2166136261;
		static constexpr uint32_t FNV_PRIME = 16777619;
		
		static constexpr uint32_t CalcHash(const char* const str, const size_t len, const uint32_t value) noexcept
		{
			return (len == 0 || *str == '\0') ? value : CalcHash(str + 1, len - 1, (value ^ (uint32_t)str[0]) * FNV_PRIME);
		}
	};
}

#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <string_view>
#include <string>
#include <glm/glm.hpp>

#include "API.hpp"

#define EG_BIT_FIELD(T) \
inline constexpr T operator|(T a, T b) noexcept \
{ return static_cast<T>(static_cast<int>(a) | static_cast<int>(b)); }\
inline constexpr T operator&(T a, T b) noexcept \
{ return static_cast<T>(static_cast<int>(a) & static_cast<int>(b)); }\
inline constexpr T& operator|=(T& a, T b) noexcept \
{ a = a | b; return a; } \
inline constexpr T& operator&=(T& a, T b) noexcept \
{ a = a & b; return a; } \
inline constexpr T operator~(T a) noexcept \
{ return static_cast<T>(~static_cast<int>(a)); }

namespace eg
{
	constexpr float PI = 3.141592653589793f;
	constexpr float TWO_PI = 6.283185307179586f;
	constexpr float HALF_PI = 1.5707963267948966f;
	
	/***
	 * Deleter for use with unique_ptr which calls std::free
	 */
	struct FreeDel
	{
		void operator()(void* mem) const noexcept
		{
			std::free(mem);
		}
	};
	
	namespace detail
	{
		EG_API extern bool devMode;
	}
	
	inline bool DevMode()
	{
		return detail::devMode;
	}
	
	EG_API void ParseCommandLineArgs(struct RunConfig& runConfig, int argc, char** argv);
	
	EG_API std::string ReadableBytesSize(uint64_t size);
	
	/***
	 * Checks the the given bitfield has a specific flag set.
	 * @tparam T The type of the bitfield enum.
	 * @param bits The bitfield to check.
	 * @param flag The flag to check for.
	 * @return Whether the bitfield has the flag set.
	 */
	template <typename T>
	inline constexpr bool HasFlag(T bits, T flag)
	{
		return (int)(bits & flag) != 0;
	}
	
	inline int8_t FloatToSNorm(float x)
	{
		return (int8_t)(glm::clamp(x, -1.0f, 1.0f) * 127.0f);
	}
	
	EG_API int64_t NanoTime();
	
	template <typename T, typename U>
	constexpr inline auto RoundToNextMultiple(T value, U multiple)
	{
		auto valModMul = value % multiple;
		return valModMul == 0 ? value : (value + multiple - valModMul);
	}
	
	inline bool FEqual(float a, float b)
	{
		return std::abs(a - b) < 1E-6f;
	}
	
	template <typename T, typename U>
	inline T AnimateTo(T value, T target, U step)
	{
		if (value < target)
			return glm::min(value + step, target);
		else
			return glm::max(value - step, target);
	}
	
	template <typename CollectionTp, typename ItemTp>
	inline bool Contains(const CollectionTp& collection, const ItemTp& item)
	{
		for (const auto& i : collection)
		{
			if (i == item)
				return true;
		}
		return false;
	}
	
	template <typename CollectionTp, typename ItemTp, typename CompareTp = std::less<ItemTp>>
	inline bool SortedContains(const CollectionTp& collection, const ItemTp& item, CompareTp compare = { })
	{
		auto it = std::lower_bound(collection.begin(), collection.end(), item, compare);
		return it != collection.end() && *it == item;
	}
	
	inline int8_t ToSNorm(float x)
	{
		return (int8_t)glm::clamp((int)(x * 127), -127, 127);
	}
	
	EG_API bool TriangleContainsPoint(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& p);
	
	EG_API std::string CanonicalPath(std::string_view path);
}

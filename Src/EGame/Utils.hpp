#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <string_view>

#include "API.hpp"
#include "Assert.hpp"

#define EG_BIT_FIELD(T)                                                                                                \
	inline constexpr T operator|(T a, T b) noexcept                                                                    \
	{                                                                                                                  \
		return static_cast<T>(static_cast<int>(a) | static_cast<int>(b));                                              \
	}                                                                                                                  \
	inline constexpr T operator&(T a, T b) noexcept                                                                    \
	{                                                                                                                  \
		return static_cast<T>(static_cast<int>(a) & static_cast<int>(b));                                              \
	}                                                                                                                  \
	inline constexpr T& operator|=(T& a, T b) noexcept                                                                 \
	{                                                                                                                  \
		a = a | b;                                                                                                     \
		return a;                                                                                                      \
	}                                                                                                                  \
	inline constexpr T& operator&=(T& a, T b) noexcept                                                                 \
	{                                                                                                                  \
		a = a & b;                                                                                                     \
		return a;                                                                                                      \
	}                                                                                                                  \
	inline constexpr T operator~(T a) noexcept                                                                         \
	{                                                                                                                  \
		return static_cast<T>(~static_cast<int>(a));                                                                   \
	}

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
	void operator()(void* mem) const noexcept { std::free(mem); }
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
	return static_cast<std::underlying_type_t<T>>(bits & flag) != 0;
}

inline int8_t FloatToSNorm(float x)
{
	return static_cast<int8_t>(glm::clamp(x, -1.0f, 1.0f) * 127.0f);
}

template <typename T>
T ReadFromPtr(const void* ptr)
{
	static_assert(std::is_trivial<T>::value);
	T value;
	std::memcpy(&value, ptr, sizeof(T));
	return value;
}

template <typename T>
T ReadFromSpan(std::span<const char> span, size_t offset)
{
	static_assert(std::is_trivial<T>::value);
	EG_ASSERT(offset + sizeof(T) <= span.size());
	T value;
	std::memcpy(&value, span.data() + offset, sizeof(T));
	return value;
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
inline bool SortedContains(const CollectionTp& collection, const ItemTp& item, CompareTp compare = {})
{
	auto it = std::lower_bound(collection.begin(), collection.end(), item, compare);
	return it != collection.end() && *it == item;
}

template <typename K, typename V>
inline V* LinearLookupMut(std::span<std::pair<K, V>> map, const K& key)
{
	for (const auto& entry : map)
	{
		if (entry.first == key)
			return &entry.second;
	}
	return nullptr;
}

template <typename K, typename V>
inline const V* LinearLookup(std::span<const std::pair<K, V>> map, const K& key)
{
	for (const auto& entry : map)
	{
		if (entry.first == key)
			return &entry.second;
	}
	return nullptr;
}

inline int8_t ToSNorm(float x)
{
	return static_cast<int8_t>(glm::clamp(static_cast<int>(std::round(x * 127.0f)), -127, 127));
}

inline uint8_t ToUNorm8(float x)
{
	return static_cast<uint8_t>(glm::clamp(static_cast<int>(std::round(x * UINT8_MAX)), 0, UINT8_MAX));
}

inline uint16_t ToUNorm16(float x)
{
	return static_cast<uint16_t>(glm::clamp(static_cast<int>(std::round(x * UINT16_MAX)), 0, UINT16_MAX));
}

EG_API bool TriangleContainsPoint(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& p);

EG_API std::string CanonicalPath(std::string_view path);

template <typename NewT, typename OldT>
inline NewT UnsignedNarrow(OldT v)
{
	static_assert(sizeof(OldT) >= sizeof(NewT));
	static_assert(std::numeric_limits<OldT>::is_integer && !std::numeric_limits<OldT>::is_signed);
	static_assert(std::numeric_limits<NewT>::is_integer && !std::numeric_limits<NewT>::is_signed);
	if (v >= std::numeric_limits<NewT>::max())
		detail::PanicImpl("UnsignedNarrow Failure");
	return static_cast<NewT>(v);
}

template <typename T>
inline std::make_unsigned_t<T> ToUnsigned(T v)
{
	if (v < 0)
		detail::PanicImpl("ToUnsigned Failure");
	return static_cast<std::make_unsigned_t<T>>(v);
}

template <typename T>
inline int ToInt(T v)
{
	static_assert(sizeof(T) >= sizeof(int));
	static_assert(std::numeric_limits<T>::is_integer);
	if (v > static_cast<T>(INT_MAX) || (std::numeric_limits<T>::is_signed && v < static_cast<T>(INT_MIN)))
		detail::PanicImpl("ToInt Failure");
	return static_cast<int>(v);
}

inline int64_t ToInt64(uint64_t v)
{
	if (v > static_cast<uint64_t>(INT64_MAX))
		detail::PanicImpl("ToInt64 Failure");
	return static_cast<int64_t>(v);
}
} // namespace eg

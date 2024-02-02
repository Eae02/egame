#pragma once

#include <bitset>

namespace eg
{
template <size_t I>
inline size_t BitsetFindFirst(const std::bitset<I>& bitset)
{
#ifdef __GNUC__
	return bitset._Find_first();
#else
	size_t i = 0;
	for (; i < I; i++)
	{
		if (bitset[i])
			break;
	}
	return i;
#endif
}

template <size_t I>
inline size_t BitsetFindNext(const std::bitset<I>& bitset, size_t pos)
{
#ifdef __GNUC__
	return bitset._Find_next(pos);
#else
	size_t i = pos + 1;
	for (; i < I; i++)
	{
		if (bitset[i])
			break;
	}
	return i;
#endif
}
} // namespace eg

#pragma once

#include "Utils.hpp"
#include "Assert.hpp"

namespace eg
{
	template <typename T>
	class Span2
	{
	public:
		Span2()
			: m_data(nullptr) { }
		
		Span2(T* data, size_t width, size_t height)
			: m_data(data), m_width(width), m_height(height), m_rowStride(width), m_colStride(1) { }
		
		Span2(T* data, size_t width, size_t height, size_t rowStride, size_t colStride)
			: m_data(data), m_width(width), m_height(height), m_rowStride(rowStride), m_colStride(colStride) { }
		
		size_t Width() const
		{ return m_width; }
		size_t Height() const
		{ return m_height; }
		
		T& At(size_t x, size_t y) const
		{
			if (x >= m_width || y >= m_height)
				EG_PANIC("Span index out of range");
			return m_data[ToIndex(x, y)];
		}
		
		T& operator[](std::pair<size_t, size_t> idx) const
		{
			return m_data[ToIndex(idx.first, idx.second)];
		}
		
		bool Empty() const
		{
			return m_width == 0 || m_height == 0;
		}
		
		Span2<T> Subspan(size_t x, size_t y, size_t width, size_t height) const
		{
			return Span2<T>(m_data + ToIndex(x, y), width, height, m_rowStride, m_colStride);
		}
		
	private:
		size_t ToIndex(size_t x, size_t y) const
		{
			return x * m_colStride + y * m_rowStride;
		}
		
		T* m_data;
		size_t m_width = 0;
		size_t m_height = 0;
		size_t m_rowStride = 0;
		size_t m_colStride = 0;
	};
}

#pragma once

#include <vector>
#include <istream>
#include <ostream>
#include <algorithm>

#include "../../Utils.hpp"
#include "../../IOUtils.hpp"
#include "../../Assert.hpp"

namespace eg
{
	template <typename T>
	struct SplineTangents
	{
		T in;
		T out;
	};
	
	enum class KeyFrameInterpolation
	{
		Linear      = 0,
		Step        = 1,
		CubicSpline = 2
	};
	
	template <typename T>
	class KeyFrameList
	{
	public:
		using SplineTangentsT = eg::SplineTangents<typename T::TransformTp>;
		static_assert(sizeof(SplineTangentsT) == sizeof(typename T::TransformTp) * 2);
		
		KeyFrameList() = default;
		
		inline KeyFrameList(KeyFrameInterpolation interpolation, std::vector<T> keyFrames)
			: m_interpolation(interpolation), m_keyFrames(keyFrames) { }
		
		void SetSplineTangents(std::vector<SplineTangentsT> tangents)
		{
			EG_ASSERT(tangents.size() == m_keyFrames.size());
			m_splineTangents = std::move(tangents);
		}
		
		inline typename T::TransformTp GetTransform(float t) const
		{
			if (m_keyFrames.empty())
				return T::DefaultTransform();
			auto it = std::lower_bound(m_keyFrames.begin(), m_keyFrames.end(), t, [&] (const T& a, float b) { return a.time < b; });
			
			if (it == m_keyFrames.begin())
				return it->transform;
			if (it == m_keyFrames.end() || m_interpolation == KeyFrameInterpolation::Step)
				return (it - 1)->transform;
			
			size_t nIdx = it - m_keyFrames.begin();
			size_t pIdx = nIdx - 1;
			
			if (m_interpolation == KeyFrameInterpolation::Linear)
			{
				return T::LinearInterpolate(m_keyFrames[pIdx], m_keyFrames[nIdx], t);
			}
			if (m_interpolation == KeyFrameInterpolation::CubicSpline)
			{
				return T::CubicSplineInterpolate(m_keyFrames[pIdx], m_keyFrames[nIdx], 
				                                 m_splineTangents[pIdx].out, m_splineTangents[nIdx].in, t);
			}
			return T::DefaultTransform();
		}
		
		inline void Write(std::ostream& stream) const
		{
			BinWrite(stream, static_cast<uint8_t>(m_interpolation));
			BinWrite(stream, UnsignedNarrow<uint32_t>(m_keyFrames.size()));
			
			for (const T& k : m_keyFrames)
				k.Write(stream);
			
			if (m_interpolation == KeyFrameInterpolation::CubicSpline)
			{
				stream.write(reinterpret_cast<const char*>(m_splineTangents.data()),
				             m_splineTangents.size() * sizeof(SplineTangentsT));
			}
		}
		
		inline void Read(std::istream& stream)
		{
			m_interpolation = static_cast<KeyFrameInterpolation>(BinRead<uint8_t>(stream));
			uint32_t count = BinRead<uint32_t>(stream);
			
			m_keyFrames.resize(count);
			for (T& k : m_keyFrames)
				k.Read(stream);
			
			if (m_interpolation == KeyFrameInterpolation::CubicSpline)
			{
				m_splineTangents.resize(count);
				stream.read(reinterpret_cast<char*>(m_splineTangents.data()), count * sizeof(SplineTangentsT));
			}
		}
		
		inline float MaxT() const
		{
			return m_keyFrames.empty() ? 0 : m_keyFrames.back().time;
		}
		
	private:
		KeyFrameInterpolation m_interpolation;
		std::vector<T> m_keyFrames;
		std::vector<SplineTangentsT> m_splineTangents;
	};
}

#pragma once

#include <algorithm>
#include <istream>
#include <ostream>
#include <vector>

#include "../../Assert.hpp"
#include "../../IOUtils.hpp"
#include "../../Utils.hpp"

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
	Linear = 0,
	Step = 1,
	CubicSpline = 2
};

template <typename T>
class KeyFrameList
{
public:
	using SplineTangentsT = eg::SplineTangents<typename T::TransformTp>;
	static_assert(sizeof(SplineTangentsT) == sizeof(typename T::TransformTp) * 2);

	static_assert(sizeof(T) == (sizeof(float) + sizeof(typename T::TransformTp)));

	KeyFrameList() = default;

	inline KeyFrameList(KeyFrameInterpolation interpolation, std::vector<T> keyFrames)
		: m_interpolation(interpolation), m_keyFrames(keyFrames)
	{
	}

	void SetSplineTangents(std::vector<SplineTangentsT> tangents)
	{
		EG_ASSERT(tangents.size() == m_keyFrames.size());
		m_splineTangents = std::move(tangents);
	}

	inline typename T::TransformTp GetTransform(float t) const
	{
		if (m_keyFrames.empty())
			return T::DefaultTransform();
		auto it = std::lower_bound(
			m_keyFrames.begin(), m_keyFrames.end(), t, [&](const T& a, float b) { return a.time < b; });

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
			return T::CubicSplineInterpolate(
				m_keyFrames[pIdx], m_keyFrames[nIdx], m_splineTangents[pIdx].out, m_splineTangents[nIdx].in, t);
		}
		return T::DefaultTransform();
	}

	inline void Write(MemoryWriter& writer) const
	{
		writer.Write(static_cast<uint8_t>(m_interpolation));
		writer.Write(UnsignedNarrow<uint32_t>(m_keyFrames.size()));
		writer.WriteBytes(
			std::span<const char>(reinterpret_cast<const char*>(m_keyFrames.data()), m_keyFrames.size() * sizeof(T)));

		if (m_interpolation == KeyFrameInterpolation::CubicSpline)
		{
			writer.WriteBytes(std::span<const char>(
				reinterpret_cast<const char*>(m_splineTangents.data()),
				m_splineTangents.size() * sizeof(SplineTangentsT)));
		}
	}

	inline void Read(MemoryReader& reader)
	{
		m_interpolation = static_cast<KeyFrameInterpolation>(reader.Read<uint8_t>());
		const uint32_t count = reader.Read<uint32_t>();

		m_keyFrames.resize(count);
		reader.ReadToSpan(std::span<char>(reinterpret_cast<char*>(m_keyFrames.data()), count * sizeof(T)));

		if (m_interpolation == KeyFrameInterpolation::CubicSpline)
		{
			m_splineTangents.resize(count);
			reader.ReadToSpan(
				std::span<char>(reinterpret_cast<char*>(m_splineTangents.data()), count * sizeof(SplineTangentsT)));
		}
	}

	inline float MaxT() const { return m_keyFrames.empty() ? 0 : m_keyFrames.back().time; }

private:
	KeyFrameInterpolation m_interpolation;
	std::vector<T> m_keyFrames;
	std::vector<SplineTangentsT> m_splineTangents;
};
} // namespace eg

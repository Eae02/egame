#pragma once

#include <glm/gtc/quaternion.hpp>
#include <istream>
#include <ostream>

#include "../../IOUtils.hpp"

namespace eg
{
template <typename _TransformTp>
struct KeyFrame
{
	using TransformTp = _TransformTp;

	float time = 0;
	TransformTp transform;

	KeyFrame() = default;

	inline void Read(std::istream& stream)
	{
		time = BinRead<float>(stream);
		stream.read(reinterpret_cast<char*>(&transform), sizeof(TransformTp));
	}

	inline void Write(std::ostream& stream) const
	{
		BinWrite(stream, time);
		stream.write(reinterpret_cast<const char*>(&transform), sizeof(TransformTp));
	}

	inline static float GetInterpol(const KeyFrame<TransformTp>& a, const KeyFrame<TransformTp>& b, float t)
	{
		return glm::clamp((t - a.time) / (b.time - a.time), 0.0f, 1.0f);
	}

	inline static TransformTp CubicSplineInterpolate(
		const KeyFrame<TransformTp>& a, const KeyFrame<TransformTp>& b, const TransformTp& aOutT,
		const TransformTp& bInT, float t)
	{
		const float x = GetInterpol(a, b, t);
		const float x2 = x * x;
		const float x3 = x2 * x;
		const float tanScale = b.time - a.time;

		return (2 * x3 - 3 * x2 + 1) * a.transform + (x3 - 2 * x2 + x) * tanScale * aOutT +
		       (-2 * x3 + 3 * x2) * b.transform + (x3 - x2) * tanScale * bInT;
	}

	bool operator<(const KeyFrame& rhs) const { return time < rhs.time; }

	bool operator>(const KeyFrame& rhs) const { return rhs < *this; }

	bool operator<=(const KeyFrame& rhs) const { return !(rhs < *this); }

	bool operator>=(const KeyFrame& rhs) const { return !(*this < rhs); }
};

struct RKeyFrame : KeyFrame<glm::quat>
{
	inline static glm::quat DefaultTransform() { return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); }

	inline static glm::quat LinearInterpolate(const KeyFrame<glm::quat>& a, const KeyFrame<glm::quat>& b, float t)
	{
		return glm::slerp(a.transform, b.transform, GetInterpol(a, b, t));
	}

	inline static glm::quat CubicSplineInterpolate(
		const KeyFrame<glm::quat>& a, const KeyFrame<glm::quat>& b, const glm::quat& aOutT, const glm::quat& bInT,
		float t)
	{
		return glm::normalize(KeyFrame::CubicSplineInterpolate(a, b, aOutT, bInT, t));
	}
};

struct Vec3KeyFrame : KeyFrame<glm::vec3>
{
	inline static glm::vec3 LinearInterpolate(const KeyFrame<glm::vec3>& a, const KeyFrame<glm::vec3>& b, float t)
	{
		return glm::mix(a.transform, b.transform, GetInterpol(a, b, t));
	}
};

struct SKeyFrame : Vec3KeyFrame
{
	inline static glm::vec3 DefaultTransform() { return glm::vec3(1.0f); }
};

struct TKeyFrame : Vec3KeyFrame
{
	inline static glm::vec3 DefaultTransform() { return glm::vec3(0.0f); }
};
} // namespace eg

#pragma once

#include "../API.hpp"
#include "AABB.hpp"
#include "Ray.hpp"

#include <glm/gtx/norm.hpp>
#include <span>

namespace eg
{
class EG_API Sphere
{
public:
	inline Sphere() : radius(0) {}

	inline Sphere(const glm::vec3& pos, float rad) : position(pos), radius(rad) {}

	static Sphere CreateEnclosing(std::span<const Sphere> spheres);

	static Sphere CreateEnclosing(std::span<const glm::vec3> positions);

	static Sphere CreateEnclosing(const AABB& box);

	Sphere Transformed(const glm::mat4& matrix) const;

	bool Intersects(const Sphere& other) const
	{
		float radiiSum = radius + other.radius;
		return glm::length2(position - other.position) < radiiSum * radiiSum;
	}

	bool Contains(const glm::vec3& pos) const { return glm::length2(position - pos) < radius * radius; }

	bool Contains(const AABB& aabb) const { return Contains(aabb.max) && Contains(aabb.min); }

	glm::vec3 position;
	float radius;
};
} // namespace eg

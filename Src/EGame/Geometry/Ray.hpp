#pragma once

#include "../API.hpp"

#include <glm/glm.hpp>

namespace eg
{
class EG_API Ray
{
public:
	Ray() = default;

	Ray(const glm::vec3& start, const glm::vec3& direction) : m_start(start), m_direction(glm::normalize(direction)) {}

	static Ray FromStartEnd(const glm::vec3& start, const glm::vec3& end)
	{
		Ray ray;
		ray.m_start = start;
		ray.m_direction = end - start;
		return ray;
	}

	static Ray UnprojectNDC(const glm::mat4& inverseViewProj, const glm::vec2& ndc);

	static Ray UnprojectScreen(const glm::mat4& inverseViewProj, const glm::vec2& screenCoords);

	float GetDistanceToPoint(const glm::vec3& point) const;

	float ProjectPoint(const glm::vec3& point) const;

	bool Intersects(const class Plane& plane, float& distance) const;

	bool Intersects(const class Sphere& sphere, float& distance) const;

	/***
	 * Returns the distance along this ray that is closest to the other ray. If parallel, returns NaN.
	 */
	float GetClosestPoint(const Ray& other);

	const glm::vec3& GetStart() const { return m_start; }

	void SetStart(const glm::vec3& start) { m_start = start; }

	const glm::vec3& GetDirection() const { return m_direction; }

	void SetDirection(const glm::vec3& direction) { m_direction = direction; }

	glm::vec3 GetPoint(float distance) const { return m_start + m_direction * distance; }

private:
	glm::vec3 m_start;
	glm::vec3 m_direction;
};
} // namespace eg

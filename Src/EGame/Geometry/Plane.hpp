#pragma once

#include "../API.hpp"
#include <glm/glm.hpp>

namespace eg
{
// Plane of points p satisfying the equation N * p = D
class EG_API Plane
{
public:
	inline Plane() : m_normal(0, 1, 0), m_distance(0) {}

	Plane(const glm::vec3& normal, float distance);

	Plane(const glm::vec3& normal, const glm::vec3& point);

	Plane(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

	glm::vec3 GetClosestPointOnPlane(const glm::vec3& point) const;

	inline float GetDistanceToPoint(const glm::vec3& pos) const { return glm::dot(m_normal, pos) - m_distance; }

	inline bool IsPointAbovePlane(const glm::vec3& pos) const { return glm::dot(m_normal, pos) > m_distance; }

	inline glm::vec3 GetAnyPointOnPlane() const { return m_normal * m_distance; }

	glm::mat3 GetTBNMatrix(glm::vec3 forward) const;

	void FlipNormal();

	inline const glm::vec3& GetNormal() const { return m_normal; }

	inline void SetNormal(const glm::vec3& normal) { m_normal = normal; }

	inline float GetDistance() const { return m_distance; }

	inline void SetDistance(float distance) { m_distance = distance; }

private:
	glm::vec3 m_normal;
	float m_distance;
};
} // namespace eg

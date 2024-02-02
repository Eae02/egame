#include "Ray.hpp"
#include "../Graphics/AbstractionHL.hpp"
#include "../Graphics/Graphics.hpp"
#include "Plane.hpp"
#include "Sphere.hpp"

namespace eg
{
Ray Ray::UnprojectScreen(const glm::mat4& inverseViewProj, const glm::vec2& screenCoords)
{
	glm::vec2 c01 = screenCoords / glm::vec2(CurrentResolutionX(), CurrentResolutionY());
	return UnprojectNDC(inverseViewProj, glm::vec2(c01.x * 2 - 1, 1 - c01.y * 2));
}

Ray Ray::UnprojectNDC(const glm::mat4& inverseViewProj, const glm::vec2& ndc)
{
	glm::vec4 nearPoint(ndc, 0, 1);
	glm::vec4 farPoint(ndc, 1, 1);

	glm::vec4 nearWorldPos4 = inverseViewProj * nearPoint;
	glm::vec4 farWorldPos4 = inverseViewProj * farPoint;

	glm::vec3 nearWorldPos = glm::vec3(nearWorldPos4) / nearWorldPos4.w;
	glm::vec3 farWorldPos = glm::vec3(farWorldPos4) / farWorldPos4.w;

	return Ray(nearWorldPos, farWorldPos - nearWorldPos);
}

float Ray::GetDistanceToPoint(const glm::vec3& point) const
{
	return glm::length(glm::cross(m_direction, point - m_start));
}

float Ray::ProjectPoint(const glm::vec3& point) const
{
	return glm::dot(point - m_start, m_direction);
}

bool Ray::Intersects(const Plane& plane, float& distance) const
{
	float div = glm::dot(plane.GetNormal(), m_direction);
	if (std::abs(div) < 1E-6f)
		return false;
	distance = (plane.GetDistance() - glm::dot(plane.GetNormal(), m_start)) / div;
	return true;
}

bool Ray::Intersects(const Sphere& sphere, float& distance) const
{
	if (sphere.Contains(m_start))
	{
		distance = 0;
		return true;
	}

	const float a = glm::length2(m_direction);
	const float b = 2.0f * glm::dot(m_direction, m_start - sphere.position);
	const float c = glm::length2(m_start - sphere.position) - sphere.radius * sphere.radius;

	const float disc = b * b - 4 * a * c;

	if (disc < 0)
		return false;

	const float discSqrt = std::sqrt(disc);
	distance = std::min((-b + discSqrt) / (2 * a), (-b - discSqrt) / (2 * a));
	return true;
}

float Ray::GetClosestPoint(const Ray& other)
{
	glm::vec3 c = other.m_start - m_start;

	float dirDot = glm::dot(m_direction, other.m_direction);
	float otherLenSq = glm::length2(other.m_direction);
	float div = glm::length2(m_direction) * otherLenSq - dirDot * dirDot;

	if (glm::abs(div) < 1E-6f)
		return std::numeric_limits<float>::quiet_NaN();

	return (-dirDot * glm::dot(other.m_direction, c) + glm::dot(m_direction, c) * otherLenSq) / div;
}
} // namespace eg

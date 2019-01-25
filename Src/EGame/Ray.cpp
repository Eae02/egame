#include "Ray.hpp"
#include "Plane.hpp"
#include "Graphics/AbstractionHL.hpp"

namespace eg
{
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
}

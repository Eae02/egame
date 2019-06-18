#include "Plane.hpp"

namespace eg
{
	Plane::Plane(const glm::vec3& normal, float distance)
	{
		const float nLength = glm::length(normal);
		m_distance = distance / nLength;
		m_normal = normal / nLength;
	}
	
	Plane::Plane(const glm::vec3& normal, const glm::vec3& point)
	{
		m_normal = glm::normalize(normal);
		m_distance = glm::dot(point, m_normal);
	}
	
	Plane::Plane(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
	{
		const glm::vec3 v1 = b - a;
		const glm::vec3 v2 = c - a;
		
		m_normal = glm::normalize(glm::cross(v1, v2));
		
		m_distance = glm::dot(a, m_normal);
	}
	
	glm::vec3 Plane::GetClosestPointOnPlane(const glm::vec3& point) const
	{
		//A plane that has the same normal as this plane and contains 'point'.
		Plane parallelPlane(m_normal, point);
		
		float distDiff = parallelPlane.m_distance - m_distance;
		return point - m_normal * distDiff;
	}
	
	glm::mat3 Plane::GetTBNMatrix(glm::vec3 forward) const
	{
		glm::vec3 tangent = glm::cross(m_normal, glm::normalize(forward));
		return glm::mat3(tangent, glm::cross(tangent, m_normal), m_normal);
	}
	
	void Plane::FlipNormal()
	{
		m_normal = -m_normal;
		m_distance = -m_distance;
	}
}

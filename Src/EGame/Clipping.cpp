#include "Clipping.hpp"
#include "Plane.hpp"

namespace eg
{
	inline bool IsZero(float x)
	{
		return abs(x) < 1E-5f;
	}
	
	float SweepPointToPlane(const glm::vec3& point, const glm::vec3& move, const Plane& plane)
	{
		float div = glm::dot(move, plane.GetNormal());
		if (IsZero(div))
			return INFINITY;
		float a = (plane.GetDistance() - glm::dot(plane.GetNormal(), point)) / div;
		return a < 0 || a > 1 ? INFINITY : a;
	}
	
	std::pair<float, glm::vec3> SweepEdgeToEdge(const glm::vec3& a1, const glm::vec3& a2, const glm::vec3& move, const glm::vec3& b1, const glm::vec3& b2)
	{
		glm::mat3 M = glm::mat3(move, b2 - b1, a1 - a2);
		if (IsZero(glm::determinant(M)))
			return { INFINITY, glm::vec3() };
		
		glm::vec3 v = glm::inverse(M) * (b1 - a1);
		if (v.x < 0 || v.x > 1)
			return { INFINITY, glm::vec3() };
		
		return std::make_pair(v.x, b1 - (b2 - b1) * v.y);
	}
	
	void CheckAABBMeshCollision(float& minDist, const eg::AABB& aabb, const glm::vec3& move,
	                            const class CollisionMesh& mesh, const glm::mat4& meshTransform)
	{
		
	}
}

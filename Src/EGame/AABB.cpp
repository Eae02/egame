#include "AABB.hpp"
#include "Utils.hpp"

namespace eg
{
	bool AABB::Contains(const glm::vec3& pos) const
	{
		return pos.x > std::fmin(min.x, max.x) &&
		       pos.x < std::fmax(min.x, max.x) &&
		       pos.y > std::fmin(min.y, max.y) &&
		       pos.y < std::fmax(min.y, max.y) &&
		       pos.z > std::fmin(min.z, max.z) &&
		       pos.z < std::fmax(min.z, max.z);
	}
	
	bool AABB::Intersects(const AABB& other) const
	{
		return !(
			std::fmin(other.min.x, other.max.x) > std::fmax(min.x, max.x) ||
			std::fmax(other.min.x, other.max.x) < std::fmin(min.x, max.x) ||
			
			std::fmin(other.min.y, other.max.y) > std::fmax(min.y, max.y) ||
			std::fmax(other.min.y, other.max.y) < std::fmin(min.y, max.y) ||
			
			std::fmin(other.min.z, other.max.z) > std::fmax(min.z, max.z) ||
			std::fmax(other.min.z, other.max.z) < std::fmin(min.z, max.z)
		        );
	}
	
	glm::vec3 AABB::NthVertex(int n) const
	{
		if (n >= 8)
			EG_PANIC("Vertex index out of range.");
		
		const bool useX2 = (n % 2) > 0;
		const bool useY2 = (n % 4) >= 2;
		const bool useZ2 = n >= 4;
		return { useX2 ? max.x : min.x, useY2 ? max.y : min.y, useZ2 ? max.z : min.z };
	}
	
	AABB AABB::TransformedBoundingBox(const glm::mat4& transform) const
	{
		AABB other;
		other.min = glm::vec3(INFINITY);
		other.max = glm::vec3(-INFINITY);
		
		auto ProcessVertex = [&] (float x, float y, float z)
		{
			glm::vec3 v(transform * glm::vec4(x, y, z, 1));
			other.min = glm::min(other.min, v);
			other.max = glm::max(other.max, v);
		};
		ProcessVertex(min.x, min.y, min.z);
		ProcessVertex(max.x, min.y, min.z);
		ProcessVertex(min.x, max.y, min.z);
		ProcessVertex(max.x, max.y, min.z);
		ProcessVertex(min.x, min.y, max.z);
		ProcessVertex(max.x, min.y, max.z);
		ProcessVertex(min.x, max.y, max.z);
		ProcessVertex(max.x, max.y, max.z);
		
		return other;
	}
}

#include "Sphere.hpp"

#include <algorithm>

namespace eg
{
	Sphere Sphere::CreateEnclosing(std::span<const Sphere> spheres)
	{
		if (spheres.empty())
			return Sphere();
		
		glm::vec3 minPos = spheres[0].position - glm::vec3(spheres[0].radius);
		glm::vec3 maxPos = spheres[0].position + glm::vec3(spheres[0].radius);
		
		std::for_each(spheres.begin() + 1, spheres.end(), [&] (const Sphere& sphere)
		{
			minPos = glm::min(minPos, sphere.position - glm::vec3(sphere.radius));
			maxPos = glm::max(maxPos, sphere.position + glm::vec3(sphere.radius));
		});
		
		const glm::vec3 sphereCenter = (minPos + maxPos) / 2.0f;
		
		float maxDistToSphereSq;
		size_t furthestSphereIndex;
		
		for (size_t i = 0; i < spheres.size(); i++)
		{
			glm::vec3 toCenter = sphereCenter - spheres[i].position;
			float distToSphereSq = glm::dot(toCenter, toCenter);
			
			if (i == 0 || distToSphereSq > maxDistToSphereSq)
			{
				maxDistToSphereSq = distToSphereSq;
				furthestSphereIndex = i;
			}
		}
		
		return Sphere(sphereCenter, std::sqrt(maxDistToSphereSq) + spheres[furthestSphereIndex].radius);
	}
	
	static glm::vec3 FurthestFrom(std::span<const glm::vec3> points, const glm::vec3& p)
	{
		float maxDist = 0;
		glm::vec3 ret = p;
		for (const glm::vec3& o : points)
		{
			float dst = glm::distance2(o, p);
			if (dst > maxDist)
			{
				maxDist = dst;
				ret = o;
			}
		}
		return ret;
	}
	
	Sphere Sphere::CreateEnclosing(std::span<const glm::vec3> positions)
	{
		if (positions.empty())
			return {};
		glm::vec3 p1 = FurthestFrom(positions, positions[0]);
		glm::vec3 p2 = FurthestFrom(positions, p1);
		return Sphere((p1 + p2) / 2.0f, glm::distance(p1, p2) / 2.0f);
	}
	
	Sphere Sphere::CreateEnclosing(const AABB& box)
	{
		return Sphere(box.Center(), glm::length(box.max - box.Center()));
	}
	
	Sphere Sphere::Transformed(const glm::mat4& matrix) const
	{
		const glm::vec3 max = position + glm::vec3(radius);
		const glm::vec3 min = position - glm::vec3(radius);
		
		const glm::vec4 transformedMax = matrix * glm::vec4(max, 1.0f);
		const glm::vec4 transformedMin = matrix * glm::vec4(min, 1.0f);
		
		const glm::vec3 transformedCenter((transformedMax + transformedMin) / 2.0f);
		const glm::vec3 toEdge = glm::vec3(transformedMax) - transformedCenter;
		
		const float maxRad = std::max(std::max(toEdge.x, toEdge.y), toEdge.z);
		
		return Sphere(transformedCenter, maxRad);
	}
}

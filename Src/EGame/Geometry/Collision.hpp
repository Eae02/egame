#pragma once

#include <glm/glm.hpp>

#include "../API.hpp"
#include "AABB.hpp"

namespace eg
{
struct CollisionEllipsoid
{
	glm::vec3 center;
	glm::vec3 radii;

	CollisionEllipsoid() = default;
	CollisionEllipsoid(glm::vec3 _center, glm::vec3 _radii) : center(_center), radii(_radii) {}

	static CollisionEllipsoid Inscribed(const AABB& aabb)
	{
		return CollisionEllipsoid(aabb.Center(), aabb.Size() / 2.0f);
	}
};

struct CollisionInfo
{
	bool collisionFound;
	glm::vec3 positionES; // The ellipsoid space position of the first intersection.
	float distance;       // The position's distance along the move vector in the range 0 to 1.

	inline CollisionInfo() : collisionFound(false), positionES(0.0f), distance(0.0f) {}

	inline CollisionInfo(glm::vec3 _positionES, float _distance)
		: collisionFound(true), positionES(_positionES), distance(_distance)
	{
	}

	inline void Min(const CollisionInfo& other)
	{
		if (other.collisionFound && (!collisionFound || other.distance < distance))
		{
			*this = other;
		}
	}
};

// Checks for collision between an ellipsoid and a mesh.
EG_API void CheckEllipsoidMeshCollision(
	CollisionInfo& info, const CollisionEllipsoid& ellipsoid, const glm::vec3& move, const class CollisionMesh& mesh,
	const glm::mat4& meshTransform);
} // namespace eg

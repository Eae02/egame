#pragma once

#include <glm/glm.hpp>

#include "API.hpp"
#include "Span.hpp"
#include "AABB.hpp"

namespace eg
{
	EG_API void CheckAABBMeshCollision(float& minDist, const eg::AABB& ellipsoid, const glm::vec3& move,
	                                   const class CollisionMesh& mesh, const glm::mat4& meshTransform);
}

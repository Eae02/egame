#pragma once

#include "../API.hpp"

#include <glm/glm.hpp>
#include <span>

namespace eg
{
class EG_API AABB
{
public:
	inline AABB() {}

	inline AABB(const glm::vec3& pos1, const glm::vec3& pos2) : min(glm::min(pos1, pos2)), max(glm::max(pos1, pos2)) {}

	bool Contains(const glm::vec3& pos) const;

	bool Contains(const AABB& aabb) const { return Contains(aabb.min) && Contains(aabb.max); }

	static AABB CreateEnclosing(std::span<const glm::vec3> positions);

	inline glm::vec3 Center() const { return (min + max) / 2.0f; }

	bool Intersects(const AABB& other) const;

	AABB TransformedBoundingBox(const glm::mat4& transform) const;

	glm::vec3 NthVertex(int n) const;

	inline glm::vec3 Size() const { return max - min; }

	glm::vec3 min;
	glm::vec3 max;
};
} // namespace eg

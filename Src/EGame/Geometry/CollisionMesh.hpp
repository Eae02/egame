#pragma once

#include "../API.hpp"
#include "AABB.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace eg
{
struct CollisionMeshCreateArgs
{
	size_t numVertices;
	const void* positionDataPtr;
	size_t positionDataStride;
	std::variant<std::span<const uint32_t>, std::span<const uint16_t>> indices;
};

class EG_API CollisionMesh
{
public:
	CollisionMesh() = default;

	explicit CollisionMesh(const CollisionMeshCreateArgs& args);

	CollisionMesh(
		std::span<const glm::vec3> vertices, std::variant<std::span<const uint32_t>, std::span<const uint16_t>> indices)
		: m_vertices(vertices.begin(), vertices.end())
	{
		SetIndices(indices);
		InitAABB();
	}

	template <typename V, typename = std::enable_if_t<std::is_same_v<decltype(std::declval<V>().position), glm::vec3>>>
	static CollisionMesh Create(
		std::span<const V> vertices, std::variant<std::span<const uint32_t>, std::span<const uint16_t>> indices)
	{
		CollisionMeshCreateArgs createArgs = {
			.numVertices = vertices.size(),
			.positionDataPtr = reinterpret_cast<const char*>(vertices.data()) + offsetof(V, position),
			.positionDataStride = sizeof(V),
			.indices = indices,
		};
		return CollisionMesh(createArgs);
	}

	static CollisionMesh Join(std::span<const CollisionMesh> meshes);

	void Transform(const glm::mat4& transform);

	void FlipWinding();

	int Intersect(const class Ray& ray, float& intersectPos, const glm::mat4* transform = nullptr) const;

	size_t NumIndices() const { return m_indices.size(); }

	size_t NumVertices() const { return m_vertices.size(); }

	std::span<const uint32_t> Indices() const { return m_indices; }

	std::span<const glm::vec3> Vertices() const { return m_vertices; }

	const glm::vec3& Vertex(size_t i) const { return m_vertices[i]; }

	const glm::vec3& VertexByIndex(size_t i) const { return Vertex(m_indices[i]); }

	const eg::AABB& BoundingBox() const { return m_aabb; }

private:
	void SetIndices(std::variant<std::span<const uint32_t>, std::span<const uint16_t>>);

	void InitAABB();

	std::vector<uint32_t> m_indices;
	std::vector<glm::vec3> m_vertices;

	AABB m_aabb;
};
} // namespace eg

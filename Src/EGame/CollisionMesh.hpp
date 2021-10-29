#pragma once

#include "AABB.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace eg
{
	class EG_API CollisionMesh
	{
	public:
		CollisionMesh() = default;
		
		template <typename V, typename I>
		static CollisionMesh Create(std::span<const V> vertices, std::span<const I> indices)
		{
			CollisionMesh mesh;
			mesh.m_vertices.resize(vertices.size());
			mesh.m_indices.resize(indices.size());
			std::copy(indices.begin(), indices.end(), mesh.m_indices.begin());
			for (size_t i = 0; i < vertices.size(); i++)
			{
				for (int j = 0; j < 3; j++)
				{
					mesh.m_vertices[i][j] = vertices[i].position[j];
				}
			}
			
			mesh.InitAABB();
			return mesh;
		}
		
		template <typename I>
		static CollisionMesh CreateV3(std::span<const glm::vec3> vertices, std::span<const I> indices)
		{
			CollisionMesh mesh;
			mesh.m_vertices.resize(vertices.size());
			mesh.m_indices.resize(indices.size());
			std::copy(vertices.begin(), vertices.end(), mesh.m_vertices.begin());
			std::copy(indices.begin(), indices.end(), mesh.m_indices.begin());
			mesh.InitAABB();
			return mesh;
		}
		
		static CollisionMesh Join(std::span<const CollisionMesh> meshes);
		
		void Transform(const glm::mat4& transform);
		
		void FlipWinding();
		
		int Intersect(const class Ray& ray, float& intersectPos, const glm::mat4* transform = nullptr) const;
		
		size_t NumIndices() const
		{
			return m_indices.size();
		}
		
		size_t NumVertices() const
		{
			return m_vertices.size();
		}
		
		std::span<const uint32_t> Indices() const
		{
			return m_indices;
		}
		
		std::span<const glm::vec3> Vertices() const
		{
			return m_vertices;
		}
		
		const glm::vec3& Vertex(size_t i) const
		{
			return m_vertices[i];
		}
		
		const glm::vec3& VertexByIndex(size_t i) const
		{
			return Vertex(m_indices[i]);
		}
		
		const eg::AABB& BoundingBox() const
		{
			return m_aabb;
		}
		
	private:
		void InitAABB();
		
		std::vector<uint32_t> m_indices;
		std::vector<glm::vec3> m_vertices;
		
		AABB m_aabb;
	};
}

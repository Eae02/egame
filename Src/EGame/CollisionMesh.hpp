#pragma once

#include "Span.hpp"
#include "SIMD.hpp"
#include "AABB.hpp"

#include <algorithm>
#include <cstdint>

namespace eg
{
	class EG_API CollisionMesh
	{
	public:
		CollisionMesh() = default;
		
		~CollisionMesh()
		{
			delete[] m_vertices;
			delete[] m_indices;
		}
		
		CollisionMesh(CollisionMesh&& other)
			: m_numVertices(other.m_numVertices), m_numIndices(other.m_numIndices),
			  m_indices(other.m_indices), m_vertices(other.m_vertices), m_aabb(other.m_aabb)
		{
			other.m_numVertices = 0;
			other.m_numIndices = 0;
			other.m_indices = nullptr;
			other.m_vertices = nullptr;
		}
		
		CollisionMesh(const CollisionMesh& other)
			: CollisionMesh(other.m_numVertices, other.m_numIndices)
		{
			std::copy_n(other.m_indices, other.m_numIndices, m_indices);
			std::copy_n(other.m_vertices, other.m_numVertices, m_vertices);
			m_aabb = other.m_aabb;
		}
		
		CollisionMesh& operator=(CollisionMesh other)
		{
			m_numVertices = other.m_numVertices;
			m_numIndices = other.m_numIndices;
			m_aabb = other.m_aabb;
			std::swap(m_vertices, other.m_vertices);
			std::swap(m_indices, other.m_indices);
			return *this;
		}
		
		template <typename V, typename I>
		static CollisionMesh Create(Span<const V> vertices, Span<const I> indices)
		{
			CollisionMesh mesh(vertices.size(), indices.size());
			std::copy(indices.begin(), indices.end(), mesh.m_indices);
			for (size_t i = 0; i < vertices.size(); i++)
			{
				for (int j = 0; j < 3; j++)
				{
					mesh.m_vertices[i][j] = vertices[i].position[j];
				}
				mesh.m_vertices[i][3] = 0;
			}
			
			mesh.InitAABB();
			return mesh;
		}
		
		template <typename I>
		static CollisionMesh CreateV3(Span<const glm::vec3> vertices, Span<const I> indices)
		{
			CollisionMesh mesh(vertices.size(), indices.size());
			std::copy(indices.begin(), indices.end(), mesh.m_indices);
			for (size_t i = 0; i < vertices.size(); i++)
			{
				for (int j = 0; j < 3; j++)
				{
					mesh.m_vertices[i][j] = vertices[i][j];
				}
				mesh.m_vertices[i][3] = 0;
			}
			
			mesh.InitAABB();
			return mesh;
		}
		
		static CollisionMesh Join(Span<const CollisionMesh> meshes);
		
		void Transform(const glm::mat4& transform);
		
		void FlipWinding();
		
		int Intersect(const class Ray& ray, float& intersectPos, const glm::mat4* transform = nullptr) const;
		
		uint32_t NumIndices() const
		{
			return m_numIndices;
		}
		
		uint32_t NumVertices() const
		{
			return m_numVertices;
		}
		
		const uint32_t* Indices() const
		{
			return m_indices;
		}
		
		const float* Vertices() const
		{
			return reinterpret_cast<const float*>(m_vertices);
		}
		
		const __m128* VerticesM128() const
		{
			return reinterpret_cast<const __m128*>(m_vertices);
		}
		
		const glm::vec3& Vertex(uint32_t i) const
		{
			return *reinterpret_cast<const glm::vec3*>(&m_vertices[i]);
		}
		
		const glm::vec3& VertexByIndex(uint32_t i) const
		{
			return Vertex(m_indices[i]);
		}
		
		const eg::AABB& BoundingBox() const
		{
			return m_aabb;
		}
		
	private:
		CollisionMesh(uint32_t numVertices, uint32_t numIndices);
		
		void InitAABB();
		
		uint32_t m_numVertices = 0;
		uint32_t m_numIndices = 0;
		uint32_t* m_indices = nullptr;
		__m128* m_vertices = nullptr;
		AABB m_aabb;
	};
}

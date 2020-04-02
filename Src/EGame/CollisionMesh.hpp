#pragma once

#include "Span.hpp"
#include "SIMD.hpp"
#include "AABB.hpp"

namespace eg
{
	class EG_API CollisionMesh
	{
	public:
		CollisionMesh() = default;
		
		template <typename V, typename I>
		static CollisionMesh Create(Span<const V> vertices, Span<const I> indices)
		{
			CollisionMesh mesh;
			mesh.m_numVertices = vertices.size();
			mesh.m_numIndices = indices.size();
			mesh.m_indices = std::make_unique<uint32_t[]>(indices.size());
			mesh.m_positions = std::make_unique<float[]>(vertices.size() * 4);
			std::copy(indices.begin(), indices.end(), mesh.m_indices.get());
			
			for (size_t i = 0; i < vertices.size(); i++)
			{
				for (int j = 0; j < 3; j++)
				{
					mesh.m_positions[i * 4 + j] = vertices[i].position[j];
				}
				mesh.m_positions[i * 4 + 3] = 0;
			}
			
			mesh.InitAABB();
			return mesh;
		}
		
		template <typename I>
		static CollisionMesh CreateV3(Span<const glm::vec3> vertices, Span<const I> indices)
		{
			CollisionMesh mesh;
			mesh.m_numVertices = vertices.size();
			mesh.m_numIndices = indices.size();
			mesh.m_indices = std::make_unique<uint32_t[]>(indices.size());
			mesh.m_positions = std::make_unique<float[]>(vertices.size() * 4);
			std::copy(indices.begin(), indices.end(), mesh.m_indices.get());
			
			for (size_t i = 0; i < vertices.size(); i++)
			{
				for (int j = 0; j < 3; j++)
				{
					mesh.m_positions[i * 4 + j] = vertices[i][j];
				}
				mesh.m_positions[i * 4 + 3] = 0;
			}
			
			mesh.InitAABB();
			return mesh;
		}
		
		void FlipWinding();
		
		int Intersect(const class Ray& ray, float& intersectPos) const;
		
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
			return m_indices.get();
		}
		
		const float* Vertices() const
		{
			return reinterpret_cast<const float*>(m_positions.get());
		}
		
#ifdef EG_HAS_SIMD
		const __m128* VerticesM128() const
		{
			return reinterpret_cast<const __m128*>(m_positions.get());
		}
#endif
		
		glm::vec3 Vertex(uint32_t i) const
		{
			return glm::vec3(m_positions[i * 4], m_positions[i * 4 + 1], m_positions[i * 4 + 2]);
		}
		
		glm::vec3 VertexByIndex(uint32_t i) const
		{
			return Vertex(m_indices[i]);
		}
		
		const eg::AABB& BoundingBox() const
		{
			return m_aabb;
		}
		
	private:
		void InitAABB();
		
		uint32_t m_numIndices = 0;
		uint32_t m_numVertices = 0;
		std::unique_ptr<uint32_t[]> m_indices;
		std::unique_ptr<float[]> m_positions;
		AABB m_aabb;
	};
}

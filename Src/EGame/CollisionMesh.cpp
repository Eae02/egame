#include "CollisionMesh.hpp"
#include "Ray.hpp"

namespace eg
{
	CollisionMesh::CollisionMesh(uint32_t numVertices, uint32_t numIndices)
		: m_numVertices(numVertices), m_numIndices(numIndices),
		  m_indices(new uint32_t[numIndices]), m_vertices(new __m128[numIndices]) { }
	
	int CollisionMesh::Intersect(const Ray& ray, float& distanceOut, const glm::mat4* transform) const
	{
		int ans = -1;
		distanceOut = INFINITY;

		alignas(16) float rayDirA[4] = { ray.GetDirection().x, ray.GetDirection().y, ray.GetDirection().z, 0.0f };
		__m128 rayDir = _mm_load_ps(rayDirA);
		
		alignas(16) float rayStartA[4] = { ray.GetStart().x, ray.GetStart().y, ray.GetStart().z, 0.0f };
		__m128 rayStart = _mm_load_ps(rayStartA);
		
		const __m128* positions;
		std::vector<glm::vec4> positionsCopy;
		if (transform != nullptr)
		{
			positionsCopy.resize(m_numVertices);
			for (size_t i = 0; i < m_numVertices; i++)
			{
				positionsCopy[i] = *transform * glm::vec4(Vertex(i), 1.0f);
				positionsCopy[i].w = 0;
			}
			positions = reinterpret_cast<const __m128*>(positionsCopy.data());
		}
		else
		{
			positions = m_vertices;
		}
		
		for (uint32_t i = 0; i < m_numIndices; i += 3)
		{
			__m128 v0 = positions[m_indices[i + 0]];
			__m128 v1 = positions[m_indices[i + 1]];
			__m128 v2 = positions[m_indices[i + 2]];

			__m128 d1 = _mm_sub_ps(v1, v0);
			__m128 d2 = _mm_sub_ps(v2, v0);
			__m128 pn = sse::Normalize(sse::Cross(d1, d2));
			float pd = sse::Dot(pn, v0);
			float dv = sse::Dot(pn, rayDir);
			float ps = sse::Dot(rayStart, pn);
			
			if (std::abs(dv) < 1E-6f)
				continue;
			
			float pdist = (pd - ps) / dv;
			if (pdist > 0 && pdist < distanceOut)
			{
				__m128 pos = _mm_add_ps(rayStart, _mm_mul_ps(rayDir, _mm_load_ps1(&pdist)));
				
				float a = sse::Dot(d1, d1);
				float b = sse::Dot(d1, d2);
				float c = sse::Dot(d2, d2);
				
				__m128 vp = _mm_sub_ps(pos, v0);
				
				float d = sse::Dot(vp, d1);
				float e = sse::Dot(vp, d2);
				
				float ac_bb = (a * c) - (b * b);
				
				float x = (d * c) - (e * b);
				float y = (e * a) - (d * b);
				float z = x + y - ac_bb;
				
				if ((reinterpret_cast<uint32_t&>(z)& ~(reinterpret_cast<uint32_t&>(x) | reinterpret_cast<uint32_t&>(y)) ) & 0x80000000)
				{
					ans = i;
					distanceOut = pdist;
				}
			}
		}
		
		return ans;
	}
	
	void CollisionMesh::Transform(const glm::mat4& transform)
	{
		for (size_t i = 0; i < m_numVertices; i++)
		{
			*reinterpret_cast<glm::vec3*>(&m_vertices[i]) = glm::vec3(transform * glm::vec4(Vertex(i), 1));
		}
		InitAABB();
	}
	
	void CollisionMesh::FlipWinding()
	{
		for (uint32_t i = 0; i < m_numIndices; i += 3)
		{
			std::swap(m_indices[i], m_indices[i + 1]);
		}
	}
	
	void CollisionMesh::InitAABB()
	{
		if (m_numVertices == 0)
			return;
		
		__m128 min = VerticesM128()[0];
		__m128 max = VerticesM128()[0];
		for (uint32_t i = 1; i < m_numVertices; i++)
		{
			min = _mm_min_ps(VerticesM128()[i], min);
			max = _mm_max_ps(VerticesM128()[i], max);
		}
		
		m_aabb.min = glm::vec3(min[0], min[1], min[2]);
		m_aabb.max = glm::vec3(max[0], max[1], max[2]);
	}
	
	CollisionMesh CollisionMesh::Join(std::span<const CollisionMesh> meshes)
	{
		CollisionMesh result;
		for (const CollisionMesh& mesh : meshes)
		{
			result.m_numIndices += mesh.m_numIndices;
			result.m_numVertices += mesh.m_numVertices;
		}
		
		result.m_vertices = new __m128[result.m_numVertices];
		result.m_indices = new uint32_t[result.m_numIndices];
		
		uint32_t nextIndex = 0;
		uint32_t nextVertex = 0;
		for (const CollisionMesh& mesh : meshes)
		{
			for (uint32_t i = 0; i < mesh.m_numIndices; i++)
			{
				result.m_indices[nextIndex++] = mesh.m_indices[i] + nextVertex;
			}
			std::copy_n(mesh.m_vertices, mesh.m_numVertices, result.m_vertices + nextVertex);
			nextVertex += mesh.m_numVertices;
		}
		
		result.InitAABB();
		return result;
	}
}

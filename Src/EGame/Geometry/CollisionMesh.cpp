#include "CollisionMesh.hpp"
#include "Ray.hpp"
#include "../Assert.hpp"

namespace eg
{
	int CollisionMesh::Intersect(const Ray& ray, float& distanceOut, const glm::mat4* transform) const
	{
		int ans = -1;
		distanceOut = INFINITY;
		
		std::vector<glm::vec3> positionsCopy;
		const glm::vec3* positions;
		if (transform != nullptr)
		{
			positionsCopy.resize(m_vertices.size());
			for (uint32_t i = 0; i < m_vertices.size(); i++)
			{
				positionsCopy[i] = *transform * glm::vec4(m_vertices[i], 1.0f);
			}
			positions = positionsCopy.data();
		}
		else
		{
			positions = m_vertices.data();
		}
		
		for (uint32_t i = 0; i < m_indices.size(); i += 3)
		{
			glm::vec3 v0 = positions[m_indices[i + 0]];
			glm::vec3 v1 = positions[m_indices[i + 1]];
			glm::vec3 v2 = positions[m_indices[i + 2]];

			glm::vec3 d1 = v1 - v0;
			glm::vec3 d2 = v2 - v0;
			glm::vec3 pn = glm::normalize(glm::cross(d1, d2));
			float pd = glm::dot(pn, v0);
			float dv = glm::dot(pn, ray.GetDirection());
			float ps = glm::dot(ray.GetStart(), pn);
			
			if (std::abs(dv) < 1E-6f)
				continue;
			
			float pdist = (pd - ps) / dv;
			if (pdist > 0 && pdist < distanceOut)
			{
				glm::vec3 pos = ray.GetPoint(pdist);
				
				float a = glm::dot(d1, d1);
				float b = glm::dot(d1, d2);
				float c = glm::dot(d2, d2);
				
				glm::vec3 vp = pos - v0;
				
				float d = glm::dot(vp, d1);
				float e = glm::dot(vp, d2);
				
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
		for (glm::vec3& v : m_vertices)
		{
			v = glm::vec3(transform * glm::vec4(v, 1));
		}
		InitAABB();
	}
	
	void CollisionMesh::FlipWinding()
	{
		for (uint32_t i = 0; i < m_indices.size(); i += 3)
		{
			std::swap(m_indices[i], m_indices[i + 1]);
		}
	}
	
	void CollisionMesh::InitAABB()
	{
		if (m_vertices.empty())
			return;
		
		glm::vec3 min = m_vertices[0];
		glm::vec3 max = m_vertices[0];
		for (uint32_t i = 1; i < m_vertices.size(); i++)
		{
			min = glm::min(m_vertices[i], min);
			max = glm::max(m_vertices[i], max);
		}
		
		m_aabb.min = glm::vec3(min[0], min[1], min[2]);
		m_aabb.max = glm::vec3(max[0], max[1], max[2]);
	}
	
	CollisionMesh CollisionMesh::Join(std::span<const CollisionMesh> meshes)
	{
		size_t totIndices = 0;
		size_t totVertices = 0;
		for (const CollisionMesh& mesh : meshes)
		{
			totIndices += mesh.m_indices.size();
			totVertices += mesh.m_vertices.size();
		}
		
		EG_ASSERT(totIndices <= UINT32_MAX);
		EG_ASSERT(totVertices <= UINT32_MAX);
		
		CollisionMesh result;
		result.m_vertices.resize(totVertices);
		result.m_indices.resize(totIndices);
		
		uint32_t nextIndex = 0;
		uint32_t nextVertex = 0;
		for (const CollisionMesh& mesh : meshes)
		{
			for (uint32_t i = 0; i < mesh.m_indices.size(); i++)
			{
				result.m_indices[nextIndex++] = mesh.m_indices[i] + nextVertex;
			}
			std::copy_n(mesh.m_vertices.data(), mesh.m_vertices.size(), result.m_vertices.data() + nextVertex);
			nextVertex += (uint32_t)mesh.m_vertices.size();
		}
		
		result.InitAABB();
		return result;
	}
}

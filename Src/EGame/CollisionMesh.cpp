#include "CollisionMesh.hpp"
#include "Ray.hpp"

namespace eg
{
	int CollisionMesh::Intersect(const Ray& ray, float& distanceOut) const
	{
		int ans = -1;
		distanceOut = INFINITY;
		
#ifdef EG_HAS_SIMD
		alignas(16) float rayDirA[4] = { ray.GetDirection().x, ray.GetDirection().y, ray.GetDirection().z, 0.0f };
		__m128 rayDir = _mm_load_ps(rayDirA);
		
		alignas(16) float rayStartA[4] = { ray.GetStart().x, ray.GetStart().y, ray.GetStart().z, 0.0f };
		__m128 rayStart = _mm_load_ps(rayStartA);
		
		const __m128* positions = VerticesM128();
#else
		const glm::vec4* positions = reinterpret_cast<const glm::vec4*>(Vertices());
#endif
		
		for (uint32_t i = 0; i < m_numIndices; i += 3)
		{
			const auto& v0 = positions[m_indices[i + 0]];
			const auto& v1 = positions[m_indices[i + 1]];
			const auto& v2 = positions[m_indices[i + 2]];

#ifdef EG_HAS_SIMD
			__m128 d1 = _mm_sub_ps(v1, v0);
			__m128 d2 = _mm_sub_ps(v2, v0);
			__m128 pn = sse::Normalize(sse::Cross(d1, d2));
			float pd = sse::Dot(pn, v0);
			float dv = sse::Dot(pn, rayDir);
			float ps = sse::Dot(rayStart, pn);
#else
			glm::vec3 d1 = v1 - v0;
			glm::vec3 d2 = v2 - v0;
			glm::vec3 pn = glm::normalize(glm::cross(d1, d2));
			float pd = glm::dot(pn, glm::vec3(v0));
			float dv = glm::dot(pn, ray.GetDirection());
			float ps = glm::dot(ray.GetStart(), pn);
#endif
			
			if (std::abs(dv) < 1E-6f)
				continue;
			
			float pdist = (pd - ps) / dv;
			if (pdist > 0 && pdist < distanceOut)
			{
#ifdef EG_HAS_SIMD
				__m128 pos = _mm_add_ps(rayStart, _mm_mul_ps(rayDir, _mm_load_ps1(&pdist)));
				
				float a = sse::Dot(d1, d1);
				float b = sse::Dot(d1, d2);
				float c = sse::Dot(d2, d2);
				
				__m128 vp = _mm_sub_ps(pos, v0);
				
				float d = sse::Dot(vp, d1);
				float e = sse::Dot(vp, d2);
#else
				glm::vec3 pos = ray.GetStart() + ray.GetDirection() * pdist;
				
				float a = glm::dot(d1, d1);
				float b = glm::dot(d1, d2);
				float c = glm::dot(d2, d2);
				
				glm::vec3 vp = pos - glm::vec3(v0);
				
				float d = glm::dot(vp, d1);
				float e = glm::dot(vp, d2);
#endif
				
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
	
	void CollisionMesh::FlipWinding()
	{
		for (uint32_t i = 0; i < m_numIndices; i += 3)
		{
			std::swap(m_indices[i], m_indices[i + 1]);
		}
	}
}

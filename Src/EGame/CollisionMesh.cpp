#include "CollisionMesh.hpp"
#include "Ray.hpp"

#include <smmintrin.h>

namespace eg
{
	inline __m128 SSECrossProduct(__m128 a, __m128 b)
	{
		return _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)), _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2))), 
			_mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2)), _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1)))
		);
	}
	
	inline float SSEDot(__m128 a, __m128 b)
	{
		return _mm_cvtss_f32(_mm_dp_ps(a, b, 0xFF));
	}
	
	inline __m128 SSENormalize(__m128 v)
	{
		return _mm_div_ps(v, _mm_sqrt_ps(_mm_dp_ps(v, v, 0xFF)));
	}
	
	int CollisionMesh::Intersect(const Ray& ray, float& distanceOut) const
	{
		int ans = -1;
		distanceOut = INFINITY;
		
		alignas(16) float rayDirA[4] = { ray.GetDirection().x, ray.GetDirection().y, ray.GetDirection().z, 0.0f };
		__m128 rayDir = _mm_load_ps(rayDirA);
		
		alignas(16) float rayStartA[4] = { ray.GetStart().x, ray.GetStart().y, ray.GetStart().z, 0.0f };
		__m128 rayStart = _mm_load_ps(rayStartA);
		
		const __m128* positions = VerticesM128();
		for (uint32_t i = 0; i < m_numIndices; i += 3)
		{
			const __m128& v0 = positions[m_indices[i + 0]];
			const __m128& v1 = positions[m_indices[i + 1]];
			const __m128& v2 = positions[m_indices[i + 2]];
			
			__m128 d1 = _mm_sub_ps(v1, v0);
			__m128 d2 = _mm_sub_ps(v2, v0);
			
			__m128 pn = SSENormalize(SSECrossProduct(d1, d2));
			float pd = SSEDot(pn, v0);
			
			float dv = SSEDot(pn, rayDir);
			if (std::abs(dv) < 1E-6f)
				continue;
			
			float ps = SSEDot(rayStart, pn);
			float pdist = (pd - ps) / dv;
			if (pdist > 0 && pdist < distanceOut)
			{
				__m128 pos = _mm_add_ps(rayStart, _mm_mul_ps(rayDir, _mm_load_ps1(&pdist)));
				
				float a = SSEDot(d1, d1);
				float b = SSEDot(d1, d2);
				float c = SSEDot(d2, d2);
				float ac_bb = (a * c) - (b * b);
				
				__m128 vp = _mm_sub_ps(pos, v0);
				
				float d = SSEDot(vp, d1);
				float e = SSEDot(vp, d2);
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

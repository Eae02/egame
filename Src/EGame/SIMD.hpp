#pragma once

#ifndef __EMSCRIPTEN__
#define EG_HAS_SIMD
#include <emmintrin.h>
#include <smmintrin.h>

namespace eg
{
	using m128 = __m128;
}

namespace eg::sse
{
	inline __m128 Cross(__m128 a, __m128 b)
	{
		return _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)), _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2))), 
			_mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2)), _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1)))
		);
	}
	
	inline float Dot(__m128 a, __m128 b)
	{
		return _mm_cvtss_f32(_mm_dp_ps(a, b, 0xFF));
	}
	
	inline __m128 Normalize(__m128 v)
	{
		return _mm_div_ps(v, _mm_sqrt_ps(_mm_dp_ps(v, v, 0xFF)));
	}
}
#else
#include <glm/glm.hpp>

namespace eg
{
	using m128 = glm::vec4;
}

namespace eg::sse
{
	inline m128 Cross(m128 a, m128 b)
	{
		return glm::vec4(glm::cross(glm::vec3(a), glm::vec3(b)), 0.0f);
	}
	
	inline float Dot(m128 a, m128 b)
	{
		return glm::dot(a, b);
	}
	
	inline m128 Normalize(m128 v)
	{
		return glm::normalize(v);
	}
}
#endif

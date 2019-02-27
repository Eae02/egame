#pragma once

#include <emmintrin.h>
#include "Span.hpp"

namespace eg
{
	class CollisionMesh
	{
	public:
		CollisionMesh() = default;
		
		template <typename V, typename I>
		static CollisionMesh Create(Span<const V> vertices, Span<const I> indices)
		{
			CollisionMesh mesh;
			mesh.m_indices.resize(indices.size());
			mesh.m_positions.resize(vertices.size());
			std::copy(indices.begin(), indices.end(), mesh.m_indices.begin());
			
			alignas(16) float setBuffer[4];
			for (size_t i = 0; i < vertices.size(); i++)
			{
				for (int j = 0; j < 3; j++)
					setBuffer[j] = vertices[i].position[j];
				mesh.m_positions = _mm_load_ps(setBuffer);
			}
		}
		
		int Intersect(const class Ray& ray, float& intersectPos) const;
		
	private:
		std::vector<uint32_t> m_indices;
		std::vector<__m128> m_positions;
	};
}

#pragma once

#include "Plane.hpp"
#include "API.hpp"

#include <emmintrin.h>

namespace eg
{
	class EG_API Frustum
	{
	public:
		Frustum() = default;
		explicit Frustum(const glm::mat4& inverseViewProj);
		
		bool Intersects(const class Sphere& sphere) const;
		
		bool Intersects(const class AABB& aabb) const;
		
		bool Contains(const class Sphere& sphere) const;
		bool Contains(const glm::vec3& point) const;
		
		inline void SetEnableZCheck(bool value)
		{ m_enableZCheck = value; }
		inline bool EnableZCheck() const
		{ return m_enableZCheck; }
		
		const Plane& GetPlane(int i) const
		{
			return m_planes[i];
		}
		
	private:
		Plane m_planes[6];
		bool m_enableZCheck = true;
	};
}

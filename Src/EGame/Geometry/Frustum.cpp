#include "Frustum.hpp"
#include "AABB.hpp"
#include "Sphere.hpp"

namespace eg
{
static Plane CreateFrustumPlane(
	const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& normalTarget)
{
	Plane plane(p1, p2, p3);

	// Flips the normal if it doesn't face the normal target (which is the center of the frustum)
	// This is to make sure that all normals face into the frustum
	if (glm::dot(plane.GetNormal(), normalTarget - p1) < 0.0f)
		plane.FlipNormal();

	return plane;
}

Frustum::Frustum(const glm::mat4& inverseViewProj)
{
	// The corners of the frustum in post-projection space
	glm::vec3 corners[8] = { { -1, 1, 0 }, { 1, 1, 0 }, { 1, -1, 0 }, { -1, -1, 0 },
		                     { -1, 1, 1 }, { 1, 1, 1 }, { 1, -1, 1 }, { -1, -1, 1 } };

	glm::vec3 frustumCenter;

	// Transforms the corners into world space and finds the center of the frustum
	for (glm::vec3& corner : corners)
	{
		glm::vec4 cornerV4 = inverseViewProj * glm::vec4(corner, 1);
		corner = glm::vec3(cornerV4.x, cornerV4.y, cornerV4.z) / cornerV4.w;

		frustumCenter += corner;
	}

	frustumCenter /= 8;

	m_planes[0] = CreateFrustumPlane(corners[3], corners[1], corners[0], frustumCenter); // Near
	m_planes[1] = CreateFrustumPlane(corners[4], corners[5], corners[7], frustumCenter); // Far
	m_planes[2] = CreateFrustumPlane(corners[0], corners[4], corners[7], frustumCenter); // Left
	m_planes[3] = CreateFrustumPlane(corners[2], corners[6], corners[1], frustumCenter); // Right
	m_planes[4] = CreateFrustumPlane(corners[5], corners[4], corners[0], frustumCenter); // Up
	m_planes[5] = CreateFrustumPlane(corners[7], corners[2], corners[3], frustumCenter); // Down
}

bool Frustum::Intersects(const Sphere& sphere) const
{
	for (int i = m_enableZCheck ? 0 : 2; i < 6; i++)
	{
		if (m_planes[i].GetDistanceToPoint(sphere.position) < -sphere.radius)
			return false;
	}

	return true;
}

bool Frustum::Intersects(const AABB& aabb) const
{
	for (int i = m_enableZCheck ? 0 : 2; i < 6; i++)
	{
		bool allOutside = true;

		for (int j = 0; j < 8; j++)
		{
			if (m_planes[i].GetDistanceToPoint(aabb.NthVertex(j)) > 0)
			{
				allOutside = false;
				break;
			}
		}

		if (allOutside)
			return false;
	}

	return true;
}

bool Frustum::Contains(const Sphere& sphere) const
{
	for (int i = m_enableZCheck ? 0 : 2; i < 6; i++)
	{
		if (m_planes[i].GetDistanceToPoint(sphere.position) < sphere.radius)
			return false;
	}

	return true;
}

bool Frustum::Contains(const glm::vec3& point) const
{
	for (int i = m_enableZCheck ? 0 : 2; i < 6; i++)
	{
		if (m_planes[i].GetDistanceToPoint(point) < 0)
			return false;
	}

	return true;
}
} // namespace eg

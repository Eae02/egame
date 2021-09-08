#include "Collision.hpp"
#include "CollisionMesh.hpp"
#include "Plane.hpp"
#include "Utils.hpp"

#include <utility>
#include <glm/gtx/norm.hpp>
#include <iostream>

namespace eg
{
	static inline float MinQuadraticRoot(float a, float b, float c, float maxR)
	{
		float det = b * b - 4.0f * a * c;
		if (det < 0)
		{
			return std::numeric_limits<float>::quiet_NaN();
		}
		
		float sqrtD = std::sqrt(det);
		float oneOverTwoA = 0.5f / a;
		
		float r1 = (-b - sqrtD) * oneOverTwoA;
		float r2 = (-b + sqrtD) * oneOverTwoA;
		
		if (r1 > r2)
		{
			std::swap(r1, r2);
		}
		
		if (r1 > 0 && r1 < maxR)
			return r1;
		if (r2 > 0 && r2 < maxR)
			return r2;
		return std::numeric_limits<float>::quiet_NaN();
	}
	
#ifndef __EMSCRIPTEN__
	void CheckEllipsoidMeshCollision(CollisionInfo& info, const CollisionEllipsoid& ellipsoid, const glm::vec3& move,
	                                 const CollisionMesh& mesh, const glm::mat4& meshTransform)
	{
		const glm::vec3 oneOverRadii = 1.0f / ellipsoid.radii;
		
		for (uint32_t i = 0; i < mesh.NumIndices(); i += 3)
		{
			glm::vec3 triVerticesES[3];
			for (int j = 0; j < 3; j++)
			{
				const glm::vec3 worldPos(meshTransform * glm::vec4(mesh.VertexByIndex(i + j), 1.0f));
				triVerticesES[j] = worldPos * oneOverRadii;
			}
			
			const glm::vec3 basePointES = ellipsoid.center * oneOverRadii;
			const glm::vec3 moveES = move * oneOverRadii;
			
			Plane plane(triVerticesES[0], triVerticesES[1], triVerticesES[2]);
			plane.FlipNormal();
			
			if (glm::dot(plane.GetNormal(), moveES) < 0.0f)
				continue;
			
			const float distToPlane = plane.GetDistanceToPoint(basePointES);
			const float NDotMove = glm::dot(plane.GetNormal(), moveES);
			
			float t0, t1;
			bool embeddedInPlane = false;
			
			if (std::abs(NDotMove) < 1E-6f)
			{
				//Sphere is moving parallel to the plane.
				if (std::abs(distToPlane) >= 1.0f)
				{
					//Sphere is above the plane, so no collision can occur.
					continue;
				}
				
				//Sphere is embedded in the plane.
				embeddedInPlane = true;
				t0 = 0.0f;
				t1 = 1.0f;
			}
			else
			{
				//The sphere is not moving parallel to the plane.
				
				t0 = (-1.0f - distToPlane) / NDotMove;
				t1 = (1.0f - distToPlane) / NDotMove;
				
				//Swaps the values so t1 > t0
				if (t0 > t1)
				{
					std::swap(t0, t1);
				}
				
				if (t0 > 1.0f || t1 < 0.0f)
				{
					//The whole intersection range is outside [0,1], no collision is possible.
					continue;
				}
				
				t0 = glm::clamp(t0, 0.0f, 1.0f);
				t1 = glm::clamp(t1, 0.0f, 1.0f);
			}
			
			if (!embeddedInPlane && (!info.collisionFound || t0 < info.distance))
			{
				//Checks for intersections with the inside of the triangle.
				const glm::vec3 planeIntersect = basePointES - plane.GetNormal() + t0 * moveES;
				if (TriangleContainsPoint(triVerticesES[0], triVerticesES[1], triVerticesES[2], planeIntersect))
				{
					info.collisionFound = true;
					info.distance = t0;
					info.positionES = planeIntersect;
					continue;
				}
			}
			
			float t = 1.0f;
			
			const float squaredMoveDistES = glm::length2(moveES);
			
			//Checks for intersections with vertices.
			for (const glm::vec3& vertex : triVerticesES)
			{
				const float a = squaredMoveDistES;
				const float b = 2.0f * glm::dot(moveES, basePointES - vertex);
				const float c = glm::distance2(vertex, basePointES) - 1.0f;
				
				const float root = MinQuadraticRoot(a, b, c, t);
				
				if (!std::isnan(root) && (!info.collisionFound || root < info.distance))
				{
					info.collisionFound = true;
					info.distance = t = root;
					info.positionES = vertex;
				}
			}
			
			//Checks for intersections with edges.
			const glm::vec3 edgeDirections[] = {
				triVerticesES[1] - triVerticesES[0],
				triVerticesES[2] - triVerticesES[1],
				triVerticesES[0] - triVerticesES[2]
			};
			for (int v = 0; v < 3; v++)
			{
				const glm::vec3 baseToVertex = triVerticesES[v] - basePointES;
				const float edgeLenSq = glm::length2(edgeDirections[v]);
				const float edgeDotMove = glm::dot(edgeDirections[v], moveES);
				const float edgeDotBaseToVertex = glm::dot(edgeDirections[v], baseToVertex);
				
				const float a = -(edgeLenSq * squaredMoveDistES) + edgeDotMove * edgeDotMove;
				const float b = 2.0f * (edgeLenSq * glm::dot(moveES, baseToVertex) - edgeDotMove * edgeDotBaseToVertex);
				const float c = edgeLenSq * (1.0f - glm::length2(baseToVertex)) +
					edgeDotBaseToVertex * edgeDotBaseToVertex;
				
				const float root = MinQuadraticRoot(a, b, c, 1.0f);
				if (std::isnan(root) || (info.collisionFound && root > info.distance))
					continue;
				
				const float f0 = (edgeDotMove * root - edgeDotBaseToVertex) / edgeLenSq;
				if (f0 >= 0.0f && f0 <= 1.0f)
				{
					info.collisionFound = true;
					info.distance = t = root;
					info.positionES = triVerticesES[v] + f0 * edgeDirections[v];
				}
			}
		}
	}
#endif
}

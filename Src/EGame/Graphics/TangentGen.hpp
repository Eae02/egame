#pragma once

#include <cstdlib>

namespace eg
{
	template <typename GetPosT, typename GetTexCoordT, typename GetNormalT, typename GetIndexT, typename SetTangentT>
	void GenerateTangents(size_t numVertices, size_t numIndices, GetPosT getPos, GetTexCoordT getTexCoord,
		GetNormalT getNormal, GetIndexT getIndex, SetTangentT setTangent)
	{
		glm::vec3* tangentsTmp = static_cast<glm::vec3*>(std::calloc(sizeof(glm::vec3), numVertices * 2));
		for (size_t i = 0; i < numIndices; i += 3)
		{
			uint32_t indices[] = { getIndex(i), getIndex(i + 1), getIndex(i + 2) };
			
			const glm::vec3 dp0 = getPos(indices[1]) - getPos(indices[0]);
			const glm::vec3 dp1 = getPos(indices[2]) - getPos(indices[0]);
			const glm::vec2 dtc0 = getTexCoord(indices[1]) - getTexCoord(indices[0]);
			const glm::vec2 dtc1 = getTexCoord(indices[2]) - getTexCoord(indices[0]);
			
			const float div = dtc0.x * dtc1.y - dtc1.x * dtc0.y;
			if (std::abs(div) < 1E-6f)
				continue;
			
			const float r = 1.0f / div;
			
			glm::vec3 d1((dtc0.y * dp0.x - dtc0.y * dp1.x) * r, (dtc1.y * dp0.y - dtc0.y * dp1.y) * r, (dtc1.y * dp0.z - dtc0.y * dp1.z) * r);
			glm::vec3 d2((dtc0.x * dp1.x - dtc1.x * dp0.x) * r, (dtc0.x * dp1.y - dtc1.x * dp0.y) * r, (dtc0.x * dp1.z - dtc1.x * dp0.z) * r);
			
			for (int j = 0; j < 3; j++)
			{
				const size_t index = indices[j] * 2;
				tangentsTmp[index] += d1;
				tangentsTmp[index + 1] += d2;
			}
		}
		
		for (size_t v = 0; v < numVertices; v++)
		{
			const size_t idx = v * 2;
			if (glm::length2(tangentsTmp[idx]) < 1E-6f)
				continue;
			
			glm::vec3 normal = getNormal(v);
			glm::vec3 tangent = glm::normalize(tangentsTmp[idx] - normal * glm::dot(normal, tangentsTmp[idx]));
			
			if (glm::dot(glm::cross(normal, tangent), tangentsTmp[idx + 1]) < 0.0f)
				tangent = -tangent;
			
			setTangent(v, tangent);
		}
		
		std::free(tangentsTmp);
	}
}

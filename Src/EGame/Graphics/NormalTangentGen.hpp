#pragma once

#include <cstdlib>
#include <memory>

#include "../Utils.hpp"

namespace eg
{
	template <typename IndexT, typename GetPosT, typename GetTexCoordT, typename GetNormalT>
	std::unique_ptr<glm::vec3[], FreeDel> GenerateTangents(
		std::span<const IndexT> indices,
		size_t numVertices, GetPosT getVertexPos, GetTexCoordT getVertexTexCoord, GetNormalT getVertexNormal)
	{
		std::unique_ptr<glm::vec3[], FreeDel> tangents(static_cast<glm::vec3*>(std::calloc(sizeof(glm::vec3), numVertices * 2)));
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			const glm::vec3 dp0 = getVertexPos(indices[i+1]) - getVertexPos(indices[i]);
			const glm::vec3 dp1 = getVertexPos(indices[i+2]) - getVertexPos(indices[i]);
			const glm::vec2 dtc0 = getVertexTexCoord(indices[i+1]) - getVertexTexCoord(indices[i]);
			const glm::vec2 dtc1 = getVertexTexCoord(indices[i+2]) - getVertexTexCoord(indices[i]);
			
			const float div = dtc0.x * dtc1.y - dtc1.x * dtc0.y;
			if (std::abs(div) < 1E-6f)
				continue;
			
			const float r = 1.0f / div;
			
			glm::vec3 d1((dtc1.y * dp0.x - dtc0.y * dp1.x) * r, (dtc1.y * dp0.y - dtc0.y * dp1.y) * r, (dtc1.y * dp0.z - dtc0.y * dp1.z) * r);
			glm::vec3 d2((dtc0.x * dp1.x - dtc1.x * dp0.x) * r, (dtc0.x * dp1.y - dtc1.x * dp0.y) * r, (dtc0.x * dp1.z - dtc1.x * dp0.z) * r);
			
			for (int j = 0; j < 3; j++)
			{
				const size_t index = indices[i + j] * 2;
				tangents[index] += d1;
				tangents[index + 1] += d2;
			}
		}
		
		for (size_t v = 0; v < numVertices; v++)
		{
			glm::vec3 tangent(0.0f);
			if (glm::length2(tangents[v * 2]) > 0.001f)
			{
				glm::vec3 normal = getVertexNormal(v);
				tangent = glm::normalize(tangents[v * 2] - normal * glm::dot(normal, tangents[v * 2]));
				if (glm::dot(glm::cross(normal, tangent), tangents[v * 2 + 1]) < 0.0f)
					tangent = -tangent;
			}
			tangents[v] = tangent;
		}
		
		return tangents;
	}
	
	template <typename IndexT, typename GetPosT>
	std::unique_ptr<glm::vec3[], FreeDel> GenerateNormals(std::span<const IndexT> indices, size_t numVertices, GetPosT getVertexPos)
	{
		std::unique_ptr<glm::vec3[], FreeDel> normals(static_cast<glm::vec3*>(std::calloc(sizeof(glm::vec3), numVertices)));
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			const glm::vec3 dp0 = getVertexPos(indices[i+1]) - getVertexPos(indices[i]);
			const glm::vec3 dp1 = getVertexPos(indices[i+2]) - getVertexPos(indices[i]);
			const glm::vec3 n = glm::normalize(glm::cross(dp0, dp1));
			for (uint32_t j = 0; j < 3; j++)
				normals[indices[i + j]] += n;
		}
		for (size_t i = 0; i < numVertices; i++)
		{
			normals[i] = glm::length2(normals[i]) > 0.001f ? glm::normalize(normals[i]) : glm::vec3(0.0f);
		}
		return normals;
	}
	
	template <typename IndexT, typename GetPosT, typename GetTexCoordT>
	std::array<std::unique_ptr<glm::vec3[], FreeDel>, 2> GenerateNormalsAndTangents(
		std::span<const IndexT> indices, size_t numVertices, GetPosT getVertexPos, GetTexCoordT getVertexTexCoord)
	{
		std::unique_ptr<glm::vec3[], FreeDel> normals = GenerateNormals(indices, numVertices, getVertexPos);
		std::unique_ptr<glm::vec3[], FreeDel> tangents = GenerateTangents(indices, numVertices, getVertexPos, getVertexTexCoord, [&] (size_t v) { return normals[v]; });
		return { std::move(normals), std::move(tangents) };
	}
}

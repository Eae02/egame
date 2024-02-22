#pragma once

#include "../API.hpp"
#include "../Hash.hpp"

#include "ModelVertexFormat.hpp"

#include <array>
#include <span>

namespace eg
{
struct StdVertex
{
	static constexpr std::string_view Name = "eg::StdVertexAos";

	float position[3];
	float texCoord[2];
	int8_t normal[4];
	int8_t tangent[4];
	uint8_t color[4];
};

struct StdVertexAnim8
{
	static constexpr std::string_view Name = "eg::StdVertexAnim8Aos";

	float position[3];
	float texCoord[2];
	int8_t normal[4];
	int8_t tangent[4];
	uint8_t color[4];
	uint8_t boneWeights[4];
	uint8_t boneIndices[4];

	EG_API void SetBoneWeights(const std::array<float, 4>& weights);
};

struct StdVertexAnim16
{
	static constexpr std::string_view Name = "eg::StdVertexAnim16Aos";

	float position[3];
	float texCoord[2];
	int8_t normal[4];
	int8_t tangent[4];
	uint8_t color[4];
	uint8_t boneWeights[4];
	uint16_t boneIndices[4];
};

EG_API void SetBoneWeights(const float weightsF[4], uint8_t weightsOut[4]);
} // namespace eg

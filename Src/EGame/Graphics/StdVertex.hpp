#pragma once

#include "../API.hpp"
#include "../Hash.hpp"

#include <span>
#include <array>

namespace eg
{
	struct StdVertex
	{
		static constexpr CTStringHash Name = "EG::StdVertex";
		
		float position[3];
		float texCoord[2];
		int8_t normal[4];
		int8_t tangent[4];
		uint8_t color[4];
	};
	
	struct StdVertexAnim8
	{
		static constexpr CTStringHash Name = "EG::StdVertexAnim8";
		
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
		static constexpr CTStringHash Name = "EG::StdVertexAnim16";
		
		float position[3];
		float texCoord[2];
		int8_t normal[4];
		int8_t tangent[4];
		uint8_t color[4];
		uint8_t boneWeights[4];
		uint16_t boneIndices[4];
	};
	
	EG_API void SetBoneWeights(const float weightsF[4], uint8_t weightsOut[4]);
}

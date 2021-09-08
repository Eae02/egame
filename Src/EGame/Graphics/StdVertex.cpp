#include "StdVertex.hpp"

namespace eg
{
	static_assert(offsetof(StdVertex, position) == offsetof(StdVertexAnim8, position));
	static_assert(offsetof(StdVertex, texCoord) == offsetof(StdVertexAnim8, texCoord));
	static_assert(offsetof(StdVertex, normal)   == offsetof(StdVertexAnim8, normal));
	static_assert(offsetof(StdVertex, tangent)  == offsetof(StdVertexAnim8, tangent));
	static_assert(offsetof(StdVertex, color)    == offsetof(StdVertexAnim8, color));
	
	static_assert(offsetof(StdVertex, position) == offsetof(StdVertexAnim16, position));
	static_assert(offsetof(StdVertex, texCoord) == offsetof(StdVertexAnim16, texCoord));
	static_assert(offsetof(StdVertex, normal)   == offsetof(StdVertexAnim16, normal));
	static_assert(offsetof(StdVertex, tangent)  == offsetof(StdVertexAnim16, tangent));
	static_assert(offsetof(StdVertex, color)    == offsetof(StdVertexAnim16, color));
	
	void SetBoneWeights(const float weightsF[4], uint8_t weightsOut[4])
	{
		float weightSum = 0;
		for (size_t i = 0; i < 4; i++)
		{
			weightSum += weightsF[i];
		}
		
		float mul = weightSum < 1E-6f ? 0.0f : 256.0f / weightSum;
		for (size_t i = 0; i < 4; i++)
		{
			weightsOut[i] = (uint8_t)glm::clamp((int)std::round(weightsF[i] * mul), 0, 255);
		}
	}
}

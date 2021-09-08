#pragma once

#include "../../EGame/Graphics/Animation/Animation.hpp"
#include "../../EGame/Graphics/Animation/Skeleton.hpp"
#include "GLTFData.hpp"

#include <nlohmann/json.hpp>

namespace eg::asset_gen::gltf
{
	Animation ImportAnimation(const GLTFData& data, const nlohmann::json& animationEl, size_t numTargets,
	                          const std::function<std::vector<int>(int)>& getTargetIndicesFromNodeIndex);
	
	Skeleton ImportSkeleton(const GLTFData& gltfData, const nlohmann::json& nodesArray, const nlohmann::json& skinEl);
}

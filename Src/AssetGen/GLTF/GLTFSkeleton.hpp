#pragma once

#include "GLTFData.hpp"

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <ostream>

namespace eg::asset_gen::gltf
{
	class GLTFSkeleton
	{
	public:
		GLTFSkeleton() = default;
		
		void Import(const GLTFData& gltfData, const nlohmann::json& nodesArray, const nlohmann::json& skinEl);
		
		int GetBoneIDByNodeIndex(int nodeIndex) const;
		
		void Write(std::ostream& stream) const;
		
		size_t BoneCount() const
		{
			return m_bones.size();
		}
	
	private:
		struct Bone
		{
			int nodeIndex;
			std::string name;
			int16_t parent; //Index can range from -1 to 255, so uint8 isn't enough.
			size_t dual;
			glm::mat4 inverseBindMatrix;
		};
		
		bool m_hasUniqueBoneNames = true;
		glm::mat4 m_rootTransform;
		std::vector<Bone> m_bones;
	};
}

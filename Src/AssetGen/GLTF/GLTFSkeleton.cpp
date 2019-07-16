#include "GLTFSkeleton.hpp"
#include "../../../Inc/Common.hpp"
#include "EGame/Utils.hpp"
#include "EGame/IOUtils.hpp"

#include <algorithm>

using namespace nlohmann;

namespace eg::asset_gen::gltf
{
	void GLTFSkeleton::Import(const GLTFData& gltfData, const json& nodesArray, const json& skinEl)
	{
		auto jointsEl = skinEl.at("joints");
		m_bones.resize(jointsEl.size());
		
		for (Bone& bone : m_bones)
		{
			bone.parent = -1;
			bone.inverseBindMatrix = glm::mat4(1.0f);
		}
		
		for (size_t i = 0; i < m_bones.size(); i++)
		{
			m_bones[i].dual = i;
			m_bones[i].nodeIndex = jointsEl[i].get<int>();
			const json& nodeEl = nodesArray.at(m_bones[i].nodeIndex);
			
			if (m_hasUniqueBoneNames)
			{
				auto nameIt = nodeEl.find("name");
				if (nameIt == nodeEl.end())
				{
					m_hasUniqueBoneNames = false;
				}
				else
				{
					const std::string& nameNT = *nameIt;
					std::string_view name = TrimString(nameNT);
					
					//Checks if any previous bones have the same name.
					if (std::any_of(m_bones.begin(), m_bones.begin() + i, [&] (const Bone& b) { return b.name == name; }))
					{
						m_hasUniqueBoneNames = false;
					}
					else
					{
						m_bones[i].name = name;
					}
				}
			}
			
			//Iterates child nodes and sets the parent field of the corresponding bone (if any).
			auto childrenIt = nodeEl.find("children");
			if (childrenIt != nodeEl.end())
			{
				for (const json& childIndexEl : *childrenIt)
				{
					//Finds the bone corresponding to this child node.
					size_t childNodeIndex = childIndexEl.get<size_t>();
					auto childBoneIt = std::find_if(jointsEl.begin(), jointsEl.end(),
						[&] (const json& j) { return j == childNodeIndex; });
					
					//If the bone was found, sets the parent field. The bone might not be found since the child node won't
					//necessarily be part of the skin.
					if (childBoneIt != jointsEl.end())
					{
						size_t childBoneIndex = childBoneIt - jointsEl.begin();
						m_bones[childBoneIndex].parent = static_cast<int16_t>(i);
					}
				}
			}
		}
		
		//Searches for dual bones
		if (m_hasUniqueBoneNames)
		{
			const char sideSeparators[] = { '.', '-', '_' };
			
			for (size_t i = 0; i < m_bones.size(); i++)
			{
				if (m_bones[i].dual != i || m_bones[i].name.size() <= 2)
					continue;
				
				const std::string& nameI = m_bones[i].name;
				
				char otherSideChar;
				switch (nameI.back())
				{
				case 'L': otherSideChar = 'R'; break;
				case 'l': otherSideChar = 'r'; break;
				case 'R': otherSideChar = 'L'; break;
				case 'r': otherSideChar = 'l'; break;
				default: otherSideChar = '\0'; break;
				}
				
				if (otherSideChar == '\0')
					continue;
				
				char separator = nameI[nameI.size() - 2];
				if (!Contains(sideSeparators, separator))
					continue;
				
				for (size_t j = 0; j < m_bones.size(); j++)
				{
					const std::string& nameJ = m_bones[j].name;
					if (nameJ.size() > 2 && nameJ.back() == otherSideChar &&
					    nameJ.compare(0, nameJ.size() - 1, nameI, 0, nameI.size() - 1) == 0)
					{
						m_bones[j].dual = i;
						m_bones[i].dual = j;
						break;
					}
				}
			}
		}
		
		//Reads inverse bind matrices
		auto inverseBindMatricesIt = skinEl.find("inverseBindMatrices");
		if (inverseBindMatricesIt != skinEl.end())
		{
			const Accessor& accessor = gltfData.GetAccessor(*inverseBindMatricesIt);
			//TODO: Validate accessor format
			
			auto inverseBindMatrices = reinterpret_cast<const glm::mat4*>(gltfData.GetAccessorData(accessor));
			
			for (size_t i = 0; i < m_bones.size(); i++)
			{
				m_bones[i].inverseBindMatrix = inverseBindMatrices[i];
			}
		}
		
		//TODO: Set m_rootTransform to the skeleton node's transform
	}
	
	int GLTFSkeleton::GetBoneIDByNodeIndex(int nodeIndex) const
	{
		for (int i = 0; i < static_cast<int>(m_bones.size()); i++)
		{
			if (m_bones[i].nodeIndex == nodeIndex)
				return i;
		}
		return -1;
	}
	/*
	void GLTFSkeleton::Write(std::ostream& stream) const
	{
		BinWrite(stream, static_cast<uint8_t>(m_bones.size()));
		if (m_bones.empty())
			return;
		
		BinWrite<uint8_t>(stream, m_hasUniqueBoneNames ? 1 : 0);
		for (const Bone& bone : m_bones)
		{
			if (m_hasUniqueBoneNames)
			{
				BinWrite<uint8_t>(stream, static_cast<uint8_t>(bone.name.size()));
				stream.write(bone.name.data(), bone.name.size());
			}
			
			BinWrite<int16_t>(stream, bone.parent);
			BinWrite<uint8_t>(stream, bone.dual);
			BinWrite(stream, bone.inverseBindMatrix);
		}
	}*/
}

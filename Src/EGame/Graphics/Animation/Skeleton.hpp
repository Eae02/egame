#pragma once

#include "../../API.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <glm/mat4x4.hpp>

namespace eg
{
	class EG_API Skeleton
	{
	public:
		Skeleton() = default;
		
		std::optional<uint32_t> GetBoneIDByName(std::string_view name) const;
		
		bool Empty() const
		{ return m_bones.empty(); }
		
		uint32_t NumBones() const
		{
			return static_cast<uint32_t>(m_bones.size());
		}
		
		bool HasUniqueBoneNames() const
		{ return m_hasUniqueBoneNames; }
		
		std::optional<uint32_t> ParentId(uint32_t boneId) const;
		
		uint32_t DualId(uint32_t boneId) const
		{
			return m_bones[boneId].dual;
		}
		
		const glm::mat4& InverseBindMatrix(uint32_t boneId) const
		{
			return m_bones[boneId].inverseBindMatrix;
		}
		
		void SetBoneParent(uint32_t boneId, std::optional<uint32_t> parentBoneId);
		
		uint32_t AddBone(std::string name, const glm::mat4& inverseBindMatrix);
		
		void InitDualBones();
		
		void Serialize(std::ostream& stream) const;
		static Skeleton Deserialize(std::istream& stream);
		
		glm::mat4 rootTransform;
		
	private:
		struct Bone
		{
			std::string name;
			uint32_t parent;
			uint32_t dual;
			glm::mat4 inverseBindMatrix;
		};
		
		bool m_hasUniqueBoneNames = true;
		std::vector<Bone> m_bones;
		std::vector<std::string> m_boneNamesSorted;
	};
}

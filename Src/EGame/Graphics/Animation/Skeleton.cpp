#include "Skeleton.hpp"
#include "../../Assert.hpp"
#include "../../IOUtils.hpp"
#include "../../Log.hpp"
#include "../../Utils.hpp"

#include <algorithm>

namespace eg
{
uint32_t Skeleton::AddBone(std::string name, const glm::mat4& inverseBindMatrix)
{
	auto nameIt = std::lower_bound(m_boneNamesSorted.begin(), m_boneNamesSorted.end(), name);
	if (nameIt != m_boneNamesSorted.end() && *nameIt == name)
		m_hasUniqueBoneNames = false;
	m_boneNamesSorted.insert(nameIt, name);

	Bone& bone = m_bones.emplace_back();
	bone.dual = UnsignedNarrow<uint8_t>(m_bones.size());
	bone.name = std::move(name);
	bone.parent = UINT32_MAX;
	bone.inverseBindMatrix = inverseBindMatrix;

	return UnsignedNarrow<uint32_t>(m_bones.size() - 1);
}

void Skeleton::InitDualBones()
{
	if (!m_hasUniqueBoneNames)
		return;
	const char sideSeparators[] = { '.', '-', '_' };

	for (size_t i = 0; i < m_bones.size(); i++)
	{
		if (m_bones[i].dual != i || m_bones[i].name.size() <= 2)
			continue;

		const std::string& nameI = m_bones[i].name;

		char otherSideChar;
		switch (nameI.back())
		{
		case 'L':
			otherSideChar = 'R';
			break;
		case 'l':
			otherSideChar = 'r';
			break;
		case 'R':
			otherSideChar = 'L';
			break;
		case 'r':
			otherSideChar = 'l';
			break;
		default:
			otherSideChar = '\0';
			break;
		}

		if (otherSideChar == '\0')
			continue;

		char separator = nameI[nameI.size() - 2];
		if (std::find(sideSeparators, sideSeparators + 3, separator) == std::end(sideSeparators))
			continue;

		for (size_t j = 0; j < m_bones.size(); j++)
		{
			const std::string& nameJ = m_bones[j].name;
			if (nameJ.size() > 2 && nameJ.back() == otherSideChar &&
			    nameJ.compare(0, nameJ.size() - 1, nameI, 0, nameI.size() - 1) == 0)
			{
				m_bones[j].dual = static_cast<uint8_t>(i);
				m_bones[i].dual = static_cast<uint8_t>(j);
				break;
			}
		}
	}
}

std::optional<uint32_t> Skeleton::ParentId(uint32_t boneId) const
{
	if (m_bones[boneId].parent == UINT32_MAX)
		return {};
	return m_bones[boneId].parent;
}

void Skeleton::SetBoneParent(uint32_t boneId, std::optional<uint32_t> parentBoneId)
{
	if (parentBoneId.has_value())
	{
		EG_ASSERT(*parentBoneId < UnsignedNarrow<uint32_t>(m_bones.size()));
		m_bones[boneId].parent = *parentBoneId;
	}
	else
	{
		m_bones[boneId].parent = UINT32_MAX;
	}
}

std::optional<uint32_t> Skeleton::GetBoneIDByName(std::string_view name) const
{
	if (!m_hasUniqueBoneNames)
	{
		Log(LogLevel::Warning, "anim", "Skeleton::GetBoneIDByName called on skeleton without unique bone names.");
		return {};
	}

	auto it = std::lower_bound(m_boneNamesSorted.begin(), m_boneNamesSorted.end(), name);
	if (it == m_boneNamesSorted.end() || *it != name)
		return {};
	return UnsignedNarrow<uint32_t>(static_cast<size_t>(it - m_boneNamesSorted.begin()));
}

void Skeleton::Serialize(std::ostream& stream) const
{
	BinWrite(stream, UnsignedNarrow<uint32_t>(m_bones.size()));
	stream.write(reinterpret_cast<const char*>(&rootTransform), sizeof(glm::mat4));

	std::vector<uint8_t> hasParent((m_bones.size() + 7) / 8, 0);
	for (size_t i = 0; i < m_bones.size(); i++)
	{
		if (m_bones[i].parent != UINT32_MAX)
			hasParent[i / 8] |= static_cast<uint8_t>(1 << (i % 8));
	}
	stream.write(reinterpret_cast<const char*>(hasParent.data()), hasParent.size());

	for (const Bone& bone : m_bones)
	{
		BinWriteString(stream, bone.name);
		if (bone.parent != UINT32_MAX)
		{
			BinWrite(stream, static_cast<uint8_t>(bone.parent));
		}
		BinWrite(stream, static_cast<uint8_t>(bone.dual));
		stream.write(reinterpret_cast<const char*>(&bone.inverseBindMatrix), sizeof(glm::mat4));
	}
}

Skeleton Skeleton::Deserialize(std::istream& stream)
{
	Skeleton skeleton;
	uint32_t numBones = BinRead<uint32_t>(stream);
	skeleton.m_bones.resize(numBones);
	skeleton.m_boneNamesSorted.resize(numBones);

	stream.read(reinterpret_cast<char*>(&skeleton.rootTransform), sizeof(glm::mat4));

	std::vector<uint8_t> hasParent((numBones + 7) / 8);
	stream.read(reinterpret_cast<char*>(hasParent.data()), hasParent.size());

	for (uint32_t i = 0; i < numBones; i++)
	{
		skeleton.m_bones[i].name = BinReadString(stream);

		if (hasParent[i / 8] & static_cast<uint8_t>(1 << (i % 8)))
			skeleton.m_bones[i].parent = BinRead<uint8_t>(stream);
		else
			skeleton.m_bones[i].parent = UINT32_MAX;

		skeleton.m_bones[i].dual = BinRead<uint8_t>(stream);
		stream.read(reinterpret_cast<char*>(&skeleton.m_bones[i].inverseBindMatrix), sizeof(glm::mat4));
		skeleton.m_boneNamesSorted[i] = skeleton.m_bones[i].name;
	}

	std::sort(skeleton.m_boneNamesSorted.begin(), skeleton.m_boneNamesSorted.end());
	for (uint32_t i = 1; i < numBones; i++)
	{
		if (skeleton.m_boneNamesSorted[i] == skeleton.m_boneNamesSorted[i - 1])
		{
			skeleton.m_hasUniqueBoneNames = false;
			break;
		}
	}

	return skeleton;
}
} // namespace eg

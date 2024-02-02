#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "../../Utils.hpp"
#include "../Model.hpp"

namespace eg
{
enum class AnimationFlags
{
	None = 0,
	SyncTransition = 1,
	MirrorLR = 2,
	Reverse = 4
};

EG_BIT_FIELD(AnimationFlags)

class EG_API AnimationDriver
{
public:
	AnimationDriver() = default;

	explicit AnimationDriver(const Model& model) : m_model(&model) {}

	void PlayLoop(
		std::string_view name, int channel, float transitionTime = 0.3f, AnimationFlags flags = AnimationFlags::None);

	void PlayOnce(std::string_view name, int channel, std::function<void()> endCallback = nullptr);

	void Update(float dt);

	const Animation* CurrentAnimation(int channel) const
	{
		auto it = m_channels.find(channel);
		return it == m_channels.end() ? nullptr : it->second.m_current.m_animation;
	}

	void SetModel(const Model& model)
	{
		m_model = &model;
		ModelChanged();
	}

	bool Ready() const { return m_targetMatrices != nullptr && m_model; }

	uint32_t NumBoneMatrices() const { return m_numBoneMatrices; }
	uint32_t NumMeshMatrices() const { return m_numMeshMatrices; }

#if __cpp_lib_shared_ptr_arrays == 201707L
	std::shared_ptr<const glm::mat4[]> BoneMatricesSP() const { return m_targetMatrices; }
#endif

	std::span<const glm::mat4> BoneMatrices() const
	{
		return std::span<const glm::mat4>(&m_targetMatrices[0], m_numBoneMatrices);
	}

	std::span<const glm::mat4> MeshMatrices() const
	{
		return std::span<const glm::mat4>(&m_targetMatrices[m_numBoneMatrices], m_numMeshMatrices);
	}

	glm::mat4 BoneMatrix(uint32_t boneId) const
	{
		if (boneId >= m_numBoneMatrices)
			return {};
		return m_targetMatrices[boneId];
	}

	glm::mat4 MeshMatrix(uint32_t meshIndex) const
	{
		if (meshIndex >= m_numMeshMatrices)
			return {};
		return m_targetMatrices[m_numBoneMatrices + meshIndex];
	}

private:
	struct ActiveAnimation
	{
		const Animation* m_animation = nullptr;
		std::string m_name;
		float m_time = 0;
		bool m_looping = false;
		bool m_mirrorLR = false;
		std::function<void()> m_endCallback;

		bool IsActive() const { return m_animation != nullptr; }

		void ModulateTime() { m_time = std::fmod(m_time, m_animation->Length()); }

		void Play(const Animation& animation, bool looping, bool mirrorLR, std::function<void()> endCallback);

		void swap(ActiveAnimation& other);
	};

	struct Channel
	{
		ActiveAnimation m_previous;
		ActiveAnimation m_current;
		ActiveAnimation m_next;

		float m_transitionElapsedTime = 0;
		float m_transitionDuration = 0;
		float m_transitionProgress = 0;
		float m_transitionProgress01 = 0;
		float m_nextTransitionTime = 0;
		float m_nSpeedScale = 0;
	};

	void UpdateAnimationPointer(ActiveAnimation& animation) const;

	void ModelChanged();

	std::vector<uint8_t> m_parentTransformApplied;
	void ApplyParentTransform(uint32_t index);

	std::map<int, Channel> m_channels;

	bool m_targetMatricesAreIdentity = false;

#if __cpp_lib_shared_ptr_arrays == 201707L
	std::shared_ptr<glm::mat4[]> m_targetMatrices;
#else
	std::unique_ptr<glm::mat4[]> m_targetMatrices;
#endif

	uint32_t m_numAllocatedTargetMatrices = 0;
	uint32_t m_numBoneMatrices = 0;
	uint32_t m_numMeshMatrices = 0;

	const Model* m_model = nullptr;
};
} // namespace eg

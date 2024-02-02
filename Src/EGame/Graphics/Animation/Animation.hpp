#pragma once

#include "KeyFrame.hpp"
#include "KeyFrameList.hpp"

namespace eg
{
// Stores a list of key frames for a set of targets. Targets are indexed starting at 0. This class doesn't concern
//  itself with what the targets are, but they are in practice either bones or whole meshes.
class EG_API Animation
{
public:
	inline explicit Animation(size_t numTargets) : m_length(0), m_targets(numTargets) {}

	void Serialize(std::ostream& stream) const;
	void Deserialize(std::istream& stream);

	void SetScaleKeyFrames(int target, KeyFrameList<SKeyFrame> keyFrames)
	{
		m_targets[target].scale = std::move(keyFrames);
		UpdateLength();
	}

	void SetRotationKeyFrames(int target, KeyFrameList<RKeyFrame> keyFrames)
	{
		m_targets[target].rotation = std::move(keyFrames);
		UpdateLength();
	}

	void SetTranslationKeyFrames(int target, KeyFrameList<TKeyFrame> keyFrames)
	{
		m_targets[target].translation = std::move(keyFrames);
		UpdateLength();
	}

	inline float Length() const { return m_length; }

	void CalcTransform(struct TRSTransform& transformOut, int target, float t) const;

	std::string name;

private:
	void UpdateLength();

	float m_length;

	// Stores the key frames for a target
	struct TargetKeyFrames
	{
		KeyFrameList<SKeyFrame> scale;
		KeyFrameList<RKeyFrame> rotation;
		KeyFrameList<TKeyFrame> translation;
	};

	std::vector<TargetKeyFrames> m_targets;
};

struct AnimationNameCompare
{
	inline bool operator()(const Animation& a, const Animation& b) { return a.name < b.name; }
	inline bool operator()(const Animation& a, std::string_view b) { return a.name < b; }
};
} // namespace eg

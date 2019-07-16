/*#include "GLTFAnimation.h"
#include "ImportedModel.h"

using namespace nlohmann;
using namespace eg::asset_gen::gltf;

inline void ReadKeyFrameTransform(glm::vec3& transformOut, const char* data, ComponentType componentType)
{
	auto dataF = reinterpret_cast<const float*>(data);
	transformOut.x = dataF[0];
	transformOut.y = dataF[1];
	transformOut.z = dataF[2];
}

inline void ReadKeyFrameTransform(glm::quat& transformOut, const char* data, ComponentType componentType)
{
	transformOut.x = ReadFNormalized(data, componentType, 0);
	transformOut.y = ReadFNormalized(data, componentType, 1);
	transformOut.z = ReadFNormalized(data, componentType, 2);
	transformOut.w = ReadFNormalized(data, componentType, 3);
}

template <typename KeyFrameTp>
inline Jade::KeyFrameList<KeyFrameTp> ReadSamplerKeyFrames(const GLTFData& data, const json& samplerEl)
{
	long inputAccessorIndex = samplerEl.at("input");
	data.CheckAccessor(inputAccessorIndex, ElementType::SCALAR, ComponentType::Float);
	const Accessor& inputAccessor = data.GetAccessor(inputAccessorIndex);
	const float* inputAccessorData = reinterpret_cast<const float*>(data.GetAccessorData(inputAccessor));
	
	const size_t numKeyFrames = inputAccessor.elementCount;
	
	const Accessor& outputAccessor = data.GetAccessor(samplerEl.at("output"));
	const char* outputAccessorData = data.GetAccessorData(outputAccessor);
	int outputDataStride = ComponentSize(outputAccessor.componentType) *
	                       ComponentsPerElement(outputAccessor.elementType);
	
	//Parses the interpolation mode
	const std::string& interpolationModeStr = samplerEl.at("interpolation");
	Jade::KeyFrameInterpolation interpolationMode;
	if (interpolationModeStr == "LINEAR")
		interpolationMode = Jade::KeyFrameInterpolation::Linear;
	else if (interpolationModeStr == "STEP")
		interpolationMode = Jade::KeyFrameInterpolation::Step;
	else if (interpolationModeStr == "CUBICSPLINE")
		interpolationMode = Jade::KeyFrameInterpolation::CubicSpline;
	else
		return {};
	
	std::vector<KeyFrameTp> keyFrames(numKeyFrames);
	
	//For a given output key frame index, stores which index to source that key frame from so that key frames become
	// sorted in ascending order of time.
	std::vector<size_t> srcIndices(keyFrames.size());
	std::iota(ALL(srcIndices), 0);
	std::sort(ALL(srcIndices), [&] (size_t a, size_t b) { return inputAccessorData[a] < inputAccessorData[b]; });
	
	size_t dataOffset = 0;
	const size_t initialOutputDataStride = outputDataStride;
	if (interpolationMode == Jade::KeyFrameInterpolation::CubicSpline)
	{
		dataOffset += outputDataStride;
		outputDataStride *= 3;
	}
	
	//Reads key frame transforms
	for (size_t i = 0; i < numKeyFrames; i++)
	{
		keyFrames[i].time = inputAccessorData[srcIndices[i]];
		const size_t offset = dataOffset + srcIndices[i] * outputDataStride;
		ReadKeyFrameTransform(keyFrames[i].transform, outputAccessorData + offset, outputAccessor.componentType);
	}
	
	Jade::KeyFrameList<KeyFrameTp> keyFrameList(interpolationMode, std::move(keyFrames));
	
	//Reads key frame spline tangents
	if (interpolationMode == Jade::KeyFrameInterpolation::CubicSpline)
	{
		std::vector<Jade::SplineTangents<typename KeyFrameTp::TransformTp>> tangents(numKeyFrames);
		for (size_t i = 0; i < numKeyFrames; i++)
		{
			const size_t offsetIn = outputDataStride * srcIndices[i];
			const size_t offsetOut = offsetIn + initialOutputDataStride * 2;
			
			ReadKeyFrameTransform(tangents[i].in, outputAccessorData + offsetIn, outputAccessor.componentType);
			ReadKeyFrameTransform(tangents[i].out, outputAccessorData + offsetOut, outputAccessor.componentType);
		}
		keyFrameList.SetSplineTangents(std::move(tangents));
	}
	
	return keyFrameList;
}

Jade::Animation ImportAnimation(const GLTFData& data, const json& animationEl, const ImportedModel& model)
{
	Jade::Animation animation(model.Meshes().size() + model.Skeleton().BoneCount());
	
	auto nameIt = animationEl.find("name");
	if (nameIt != animationEl.end())
	{
		animation.SetName(*nameIt);
	}
	
	const json& samplersArray = animationEl.at("samplers");
	
	for (const json& channelEl : animationEl.at("channels"))
	{
		auto targetIt = channelEl.find("target");
		if (targetIt == channelEl.end())
			continue;
		
		auto nodeIt = targetIt->find("node");
		if (nodeIt == targetIt->end())
			continue;
		
		std::vector<int> targets = model.GetTargetIndicesFromNodeIndex(*nodeIt);
		if (targets.empty())
			continue;
		
		const json& samplerEl = samplersArray.at(channelEl.at("sampler").get<int>());
		
		const std::string& pathString = targetIt->at("path");
		if (pathString == "translation")
		{
			auto keyFrames = ReadSamplerKeyFrames<Jade::TKeyFrame>(data, samplerEl);
			for (int target : targets)
				animation.SetTranslationKeyFrames(target, keyFrames);
		}
		else if (pathString == "scale")
		{
			auto keyFrames = ReadSamplerKeyFrames<Jade::SKeyFrame>(data, samplerEl);
			for (int target : targets)
				animation.SetScaleKeyFrames(target, keyFrames);
		}
		else if (pathString == "rotation")
		{
			auto keyFrames = ReadSamplerKeyFrames<Jade::RKeyFrame>(data, samplerEl);
			for (int target : targets)
				animation.SetRotationKeyFrames(target, keyFrames);
		}
	}
	
	return animation;
}
*/

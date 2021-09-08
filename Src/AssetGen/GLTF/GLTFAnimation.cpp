#include "GLTFAnimation.hpp"

using namespace nlohmann;

namespace eg::asset_gen::gltf
{
	void ReadKeyFrameTransform(glm::vec3& transformOut, const char* data, ComponentType componentType)
	{
		auto dataF = reinterpret_cast<const float*>(data);
		transformOut.x = dataF[0];
		transformOut.y = dataF[1];
		transformOut.z = dataF[2];
	}
	
	void ReadKeyFrameTransform(glm::quat& transformOut, const char* data, ComponentType componentType)
	{
		transformOut.x = ReadFNormalized(data, componentType, 0);
		transformOut.y = ReadFNormalized(data, componentType, 1);
		transformOut.z = ReadFNormalized(data, componentType, 2);
		transformOut.w = ReadFNormalized(data, componentType, 3);
	}
	
	template <typename KeyFrameTp>
	KeyFrameList<KeyFrameTp> ReadSamplerKeyFrames(const GLTFData& data, const json& samplerEl)
	{
		long inputAccessorIndex = samplerEl.at("input");
		data.CheckAccessor(inputAccessorIndex, ElementType::SCALAR, ComponentType::Float);
		const Accessor& inputAccessor = data.GetAccessor(inputAccessorIndex);
		const float* inputAccessorData = reinterpret_cast<const float*>(data.GetAccessorData(inputAccessor));
		
		const size_t numKeyFrames = inputAccessor.elementCount;
		
		const Accessor& outputAccessor = data.GetAccessor(samplerEl.at("output"));
		const char* outputAccessorData = data.GetAccessorData(outputAccessor);
		int outputDataStride = ComponentSize(outputAccessor.componentType) * ComponentsPerElement(outputAccessor.elementType);
		
		//Parses the interpolation mode
		const std::string& interpolationModeStr = samplerEl.at("interpolation");
		KeyFrameInterpolation interpolationMode;
		if (interpolationModeStr == "LINEAR")
			interpolationMode = KeyFrameInterpolation::Linear;
		else if (interpolationModeStr == "STEP")
			interpolationMode = KeyFrameInterpolation::Step;
		else if (interpolationModeStr == "CUBICSPLINE")
			interpolationMode = KeyFrameInterpolation::CubicSpline;
		else
			return {};
		
		std::vector<KeyFrameTp> keyFrames(numKeyFrames);
		
		//For a given output key frame index, stores which index to source that key frame from so that key frames become
		// sorted in ascending order of time.
		std::vector<size_t> srcIndices(keyFrames.size());
		std::iota(srcIndices.begin(), srcIndices.end(), 0);
		std::sort(srcIndices.begin(), srcIndices.end(), [&] (size_t a, size_t b)
		{
			return inputAccessorData[a] < inputAccessorData[b];
		});
		
		size_t dataOffset = 0;
		const size_t initialOutputDataStride = outputDataStride;
		if (interpolationMode == KeyFrameInterpolation::CubicSpline)
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
		
		KeyFrameList<KeyFrameTp> keyFrameList(interpolationMode, std::move(keyFrames));
		
		//Reads key frame spline tangents
		if (interpolationMode == KeyFrameInterpolation::CubicSpline)
		{
			std::vector<SplineTangents<typename KeyFrameTp::TransformTp>> tangents(numKeyFrames);
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
	
	Animation ImportAnimation(const GLTFData& data, const json& animationEl, size_t numTargets,
	                          const std::function<std::vector<int>(int)>& getTargetIndicesFromNodeIndex)
	{
		Animation animation(numTargets);
		
		auto nameIt = animationEl.find("name");
		if (nameIt != animationEl.end())
		{
			animation.name = *nameIt;
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
			
			std::vector<int> targets = getTargetIndicesFromNodeIndex(*nodeIt);
			if (targets.empty())
				continue;
			
			const json& samplerEl = samplersArray.at(channelEl.at("sampler").get<int>());
			
			const std::string& pathString = targetIt->at("path");
			if (pathString == "translation")
			{
				auto keyFrames = ReadSamplerKeyFrames<TKeyFrame>(data, samplerEl);
				for (int target : targets)
					animation.SetTranslationKeyFrames(target, keyFrames);
			}
			else if (pathString == "scale")
			{
				auto keyFrames = ReadSamplerKeyFrames<SKeyFrame>(data, samplerEl);
				for (int target : targets)
					animation.SetScaleKeyFrames(target, keyFrames);
			}
			else if (pathString == "rotation")
			{
				auto keyFrames = ReadSamplerKeyFrames<RKeyFrame>(data, samplerEl);
				for (int target : targets)
					animation.SetRotationKeyFrames(target, keyFrames);
			}
		}
		
		return animation;
	}
	
	Skeleton ImportSkeleton(const GLTFData& gltfData, const json& nodesArray, const json& skinEl)
	{
		Skeleton skeleton;
		
		const glm::mat4* inverseBindMatrices = nullptr;
		auto inverseBindMatricesIt = skinEl.find("inverseBindMatrices");
		if (inverseBindMatricesIt != skinEl.end())
		{
			const Accessor& accessor = gltfData.GetAccessor(*inverseBindMatricesIt);
			//TODO: Validate accessor format
			
			inverseBindMatrices = reinterpret_cast<const glm::mat4*>(gltfData.GetAccessorData(accessor));
		}
		
		auto jointsEl = skinEl.at("joints");
		std::vector<std::optional<uint32_t>> boneParentIds(jointsEl.size());
		for (size_t i = 0; i < jointsEl.size(); i++)
		{
			const json& nodeEl = nodesArray.at(jointsEl[i].get<int>());
			auto nameIt = nodeEl.find("name");
			std::string name = nameIt == nodeEl.end() ? "" : *nameIt;
			
			skeleton.AddBone(std::move(name), inverseBindMatrices ? inverseBindMatrices[i] : glm::mat4(1.0f));
			
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
					
					//If the bone was found, sets the parent field.
					//The bone might not be found since the child node won't necessarily be part of the skin.
					if (childBoneIt != jointsEl.end())
					{
						boneParentIds.at(childBoneIt - jointsEl.begin()) = i;
					}
				}
			}
		}
		
		for (size_t i = 0; i < jointsEl.size(); i++)
		{
			skeleton.SetBoneParent(i, boneParentIds[i]);
		}
		
		skeleton.InitDualBones();
		
		//TODO: Set skeleton.rootTransform to the skeleton node's transform
		
		return skeleton;
	}
}

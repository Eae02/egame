#include "AnimationDriver.hpp"
#include "TRSTransform.hpp"
#include "../../Log.hpp"

namespace eg
{
	void AnimationDriver::PlayLoop(std::string_view name, int channel, float transitionTime, AnimationFlags flags)
	{
		if (m_model == nullptr)
			return;
		
		const Animation* animation = m_model->FindAnimation(name);
		if (animation == nullptr)
		{
			Log(LogLevel::Error, "anim", "Animation not found '{0}'", name);
			return;
		}
		
		auto channelIt = m_channels.find(channel);
		if (channelIt == m_channels.end())
		{
			channelIt = m_channels.emplace(channel, Channel()).first;
		}
		
		bool mirrorLR = HasFlag(flags, AnimationFlags::MirrorLR);
		
		if (animation == channelIt->second.m_current.m_animation && mirrorLR == channelIt->second.m_current.m_mirrorLR)
			return;
		
		if (HasFlag(flags, AnimationFlags::SyncTransition))
		{
			channelIt->second.m_next.Play(*animation, true, mirrorLR, nullptr);
			channelIt->second.m_nextTransitionTime = transitionTime;
		}
		else
		{
			channelIt->second.m_previous = std::move(channelIt->second.m_current);
			channelIt->second.m_current.Play(*animation, true, mirrorLR, nullptr);
			
			channelIt->second.m_transitionDuration = transitionTime;
			channelIt->second.m_transitionElapsedTime = 0;
			channelIt->second.m_transitionProgress = channelIt->second.m_previous.m_time;
			channelIt->second.m_nSpeedScale = 1.0f;
		}
	}
	
	void AnimationDriver::PlayOnce(std::string_view name, int channel, std::function<void()> endCallback)
	{
		if (m_model == nullptr)
			return;
		
		const Animation* animation = m_model->FindAnimation(name);
		if (animation == nullptr)
		{
			Log(LogLevel::Error, "anim", "Animation not found '{0}'", name);
			return;
		}
		
		auto channelIt = m_channels.find(channel);
		if (channelIt == m_channels.end())
		{
			channelIt = m_channels.emplace(channel, Channel()).first;
		}
		
		channelIt->second.m_next.Play(*animation, false, false, std::move(endCallback));
	}
	
	void AnimationDriver::Update(float dt)
	{
		if (m_model == nullptr)
			return;
		
		m_numBoneMatrices = m_model->skeleton.NumBones();
		m_numMeshMatrices = UnsignedNarrow<uint32_t>(m_model->NumMeshes());
		const uint32_t numTargets = m_numBoneMatrices + m_numMeshMatrices;
		if (numTargets != m_numAllocatedTargetMatrices)
		{
#if __cpp_lib_shared_ptr_arrays == 201707L
			m_targetMatrices = std::make_shared<glm::mat4[]>(numTargets);
#else
			m_targetMatrices = std::make_unique<glm::mat4[]>(numTargets);
#endif
			m_numAllocatedTargetMatrices = numTargets;
			m_targetMatricesAreIdentity = false;
		}
		
		if (m_channels.empty())
		{
			if (!m_targetMatricesAreIdentity)
			{
				std::fill_n(m_targetMatrices.get(), numTargets, glm::mat4(1.0f));
				m_targetMatricesAreIdentity = true;
			}
			return;
		}
		
		m_targetMatricesAreIdentity = false;
		
		for (auto it = m_channels.begin(); it != m_channels.end();)
		{
			Channel& channel = it->second;
			if (!channel.m_current.IsActive())
			{
				if (channel.m_next.IsActive())
				{
					channel.m_current.swap(channel.m_next);
				}
				else
				{
					it = m_channels.erase(it);
					continue;
				}
			}
			
			channel.m_current.m_time += dt;
			bool pastEnd = channel.m_current.m_time >= channel.m_current.m_animation->Length();
			if (pastEnd)
			{
				//channel.second.m_current.m_endCallback();
				//if (!channel.second.m_current.m_looping)
				
				channel.m_current.ModulateTime();
			}
			
			if (channel.m_next.IsActive())
			{
				//If there is a next animation and no previous being transitioned out of,
				// start transitioning to the next animation.
				if (!channel.m_previous.IsActive() && pastEnd)
				{
					channel.m_transitionDuration = channel.m_nextTransitionTime;
					channel.m_previous = std::move(channel.m_current);
					channel.m_current = std::move(channel.m_next);
					channel.m_next = { };
					
					channel.m_transitionElapsedTime = 0;
					channel.m_transitionProgress = channel.m_previous.m_time;
					channel.m_nSpeedScale = channel.m_current.m_animation->Length() / channel.m_previous.m_animation->Length();
				}
			}
			
			if (channel.m_previous.IsActive())
			{
				channel.m_transitionElapsedTime += dt;
				if (channel.m_transitionElapsedTime >= channel.m_transitionDuration)
				{
					//The transition is done playing, so switch over to the new animation.
					channel.m_current.m_time = channel.m_transitionProgress * channel.m_nSpeedScale;
					channel.m_current.ModulateTime();
					channel.m_previous = { };
				}
				else
				{
					channel.m_transitionProgress01 = channel.m_transitionElapsedTime / channel.m_transitionDuration;
					float s = glm::mix(1.0f, 1.0f / channel.m_nSpeedScale, channel.m_transitionProgress01);
					channel.m_transitionProgress += dt * s;
				}
			}
			
			++it;
		}
		
		//Generates transform matrices for all targets (both bones and meshes) and stores these in m_targetMatrices.
		// Bone transforms are generated relative to their parents.
		for (uint32_t i = 0; i < numTargets; i++)
		{
			TRSTransform transform;
			
			auto CalcTransform = [&] (TRSTransform& transformOut, const ActiveAnimation& animation, float t)
			{
				//The index of the target to take this transform from. If mirroring is not enabled, this is the target
				// itself, but otherwise (only for bones) it is the bone on the other side of the mesh.
				uint32_t srcTarget = i;
				if (animation.m_mirrorLR && i < m_numBoneMatrices)
					srcTarget = m_model->skeleton.DualId(srcTarget);
				
				animation.m_animation->CalcTransform(transformOut, srcTarget, t);
				
				if (animation.m_mirrorLR)
				{
					//TODO: Don't hardcode mirror to be along the X-axis
					transformOut.rotation.y = -transformOut.rotation.y;
					transformOut.rotation.z = -transformOut.rotation.z;
					transformOut.translation.x *= -1;
				}
			};
			
			for (const auto& channelP : m_channels)
			{
				const Channel& channel = channelP.second;
				
				if (channel.m_previous.IsActive())
				{
					TRSTransform oTransform, nTransform;
					
					float ot = std::fmod(channel.m_transitionProgress, channel.m_previous.m_animation->Length());
					CalcTransform(oTransform, channel.m_previous, ot);
					
					float nt = std::fmod(channel.m_transitionProgress * channel.m_nSpeedScale, channel.m_current.m_animation->Length());
					CalcTransform(nTransform, channel.m_current, nt);
					
					transform = oTransform.Interpolate(nTransform, channel.m_transitionProgress01);
				}
				else
				{
					CalcTransform(transform, channel.m_current, channel.m_current.m_time);
				}
			}
			
			m_targetMatrices[i] = transform.GetMatrix();
		}
		
		//Cascades bone parent transforms through the transform buffer
		if (m_parentTransformApplied.size() < m_numBoneMatrices)
			m_parentTransformApplied.resize(m_numBoneMatrices);
		std::fill_n(m_parentTransformApplied.data(), m_numBoneMatrices, 0);
		for (uint32_t i = 0; i < m_numBoneMatrices; i++)
		{
			if (!m_parentTransformApplied[i])
				ApplyParentTransform(i);
		}
		
		//Applies the inverse bind matrix to each bone
		for (uint32_t i = 0; i < m_numBoneMatrices; i++)
		{
			m_targetMatrices[i] = m_targetMatrices[i] * m_model->skeleton.InverseBindMatrix(i);
		}
	}
	
	void AnimationDriver::ActiveAnimation::Play(const Animation& animation, bool looping, bool mirrorLR, std::function<void()> endCallback)
	{
		m_animation = &animation;
		m_name = animation.name;
		m_time = 0;
		m_mirrorLR = mirrorLR;
		m_looping = looping;
		m_endCallback = std::move(endCallback);
	}
	
	void AnimationDriver::ActiveAnimation::swap(ActiveAnimation& other)
	{
		std::swap(m_animation, other.m_animation);
		std::swap(m_time, other.m_time);
		std::swap(m_looping, other.m_looping);
		m_name.swap(other.m_name);
		m_endCallback.swap(other.m_endCallback);
	}
	
	void AnimationDriver::UpdateAnimationPointer(ActiveAnimation& animation) const
	{
		if (animation.m_name.empty())
			return;
		if (m_model != nullptr)
			animation.m_animation = m_model->FindAnimation(animation.m_name);
		else
			animation.m_animation = nullptr;
	}
	
	void AnimationDriver::ModelChanged()
	{
		for (auto& channel : m_channels)
		{
			UpdateAnimationPointer(channel.second.m_previous);
			UpdateAnimationPointer(channel.second.m_current);
			UpdateAnimationPointer(channel.second.m_next);
		}
	}
	
	//DP function which turns bone transforms in m_boneMatrices
	// from being relative to their parent to be relative to the root bone.
	void AnimationDriver::ApplyParentTransform(uint32_t index)
	{
		if (std::optional<uint32_t> parentId = m_model->skeleton.ParentId(index))
		{
			if (!m_parentTransformApplied[*parentId])
			{
				ApplyParentTransform(*parentId);
			}
			
			m_targetMatrices[index] = m_targetMatrices[*parentId] * m_targetMatrices[index];
		}
		
		m_parentTransformApplied[index] = true;
	}
}

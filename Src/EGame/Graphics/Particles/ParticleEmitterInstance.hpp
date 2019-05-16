#pragma once

#include "../../API.hpp"
#include "ParticleManager.hpp"

namespace eg
{
	class EG_API ParticleEmitterInstance
	{
	public:
		friend class ParticleManager;
		
		ParticleEmitterInstance()
			: m_id(0), m_manager(nullptr) { }
		
		~ParticleEmitterInstance()
		{
			Kill();
		}
		
		ParticleEmitterInstance(ParticleEmitterInstance&& other) noexcept
			: m_id(other.m_id), m_manager(other.m_manager)
		{
			other.m_manager = nullptr;
		}
		
		ParticleEmitterInstance& operator=(ParticleEmitterInstance&& other) noexcept
		{
			Kill();
			m_manager = other.m_manager;
			m_id = other.m_id;
			other.m_manager = nullptr;
			return *this;
		}
		
		ParticleEmitterInstance(const ParticleEmitterInstance& other) = delete;
		ParticleEmitterInstance& operator=(const ParticleEmitterInstance& other) = delete;
		
		void Kill()
		{
			if (m_manager != nullptr)
			{
				m_manager->GetEmitter(m_id).alive = false;
				m_manager = nullptr;
			}
		}
		
		void SetTransform(const glm::mat4& transform)
		{
			m_manager->GetEmitter(m_id).transform = transform;
		}
		
		bool Alive() const
		{
			return m_manager != nullptr;
		}
		
		float EmissionRateFactor() const
		{
			return m_emissionRateFactor;
		}
		
		void SetEmissionRateFactor(float emissionRateFactor)
		{
			m_manager->GetEmitter(m_id).UpdateEmissionDelay(emissionRateFactor);
			m_emissionRateFactor = emissionRateFactor;
		}
		
	private:
		ParticleEmitterInstance(uint32_t id, ParticleManager* manager)
			: m_id(id), m_manager(manager) { }
		
		uint32_t m_id;
		float m_emissionRateFactor;
		ParticleManager* m_manager;
	};
}

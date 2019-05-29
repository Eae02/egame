#pragma once

#include "../API.hpp"
#include "../Graphics/Particles/ParticleEmitterInstance.hpp"

namespace eg
{
	class EG_API ECParticleSystem
	{
	public:
		explicit ECParticleSystem(ParticleManager* manager = nullptr)
			: m_manager(manager) { }
		
		ECParticleSystem(ECParticleSystem&&) = default;
		ECParticleSystem(const ECParticleSystem&) = delete;
		ECParticleSystem& operator=(ECParticleSystem&&) = default;
		ECParticleSystem& operator=(const ECParticleSystem&) = delete;
		
		void AddEmitter(const ParticleEmitterType& emitterType)
		{
			m_emitters.push_back(m_manager->AddEmitter(emitterType));
		}
		
		Span<const ParticleEmitterInstance> Emitters() const
		{
			return m_emitters;
		}
		
		Span<ParticleEmitterInstance> Emitters()
		{
			return m_emitters;
		}
		
		void ClearEmitters()
		{
			m_emitters.clear();
		}
		
		static void Update(class EntityManager& entityManager);
		
	private:
		ParticleManager* m_manager;
		std::vector<ParticleEmitterInstance> m_emitters;
	};
}

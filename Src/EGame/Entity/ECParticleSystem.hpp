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
		
		static void Update(class EntityManager& entityManager);
		
	private:
		ParticleManager* m_manager;
		std::vector<ParticleEmitterInstance> m_emitters;
	};
}

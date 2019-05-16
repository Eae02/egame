#include "ECParticleSystem.hpp"
#include "EntitySignature.hpp"
#include "EntityManager.hpp"
#include "ECTransform.hpp"

namespace eg
{
	static EntitySignature ParticleSystemSignature = EntitySignature::Create<ECParticleSystem>();
	
	void ECParticleSystem::Update(EntityManager& entityManager)
	{
		for (Entity& entity : entityManager.GetEntitySet(ParticleSystemSignature))
		{
			glm::mat4 transform = GetEntityTransform3D(entity);
			for (ParticleEmitterInstance& emitter : entity.GetComponent<ECParticleSystem>().m_emitters)
			{
				emitter.SetTransform(transform);
			}
		}
	}
}

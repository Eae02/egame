#include "EntitySet.hpp"

namespace eg
{
	void EntitySet::RemoveDead()
	{
		for (int64_t i = m_entities.size() - 1; i >= 0; i--)
		{
			if (m_entities[i].Get() == nullptr)
			{
				m_entities[i] = m_entities.back();
				m_entities.pop_back();
			}
		}
	}
	
	void EntitySet::MaybeAdd(const Entity& entity)
	{
		if (m_signature->IsSubsetOf(entity.Signature()))
			m_entities.emplace_back(entity);
	}
}

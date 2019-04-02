#include "EntitySignature.hpp"
#include "../Utils.hpp"
#include "../Log.hpp"

namespace eg
{
	void EntitySignature::EndInit()
	{
		std::sort(m_componentTypes.begin(), m_componentTypes.end());
		
		m_hash = 0;
		for (ComponentType& component : m_componentTypes)
		{
			HashAppend(m_hash, component.typeIndex.hash_code());
		}
	}
	
	bool EntitySignature::IsSubsetOf(const EntitySignature& other) const
	{
		size_t otherIdx = 0;
		size_t thisIdx = 0;
		while (thisIdx < m_componentTypes.size() && otherIdx < other.m_componentTypes.size())
		{
			if (m_componentTypes[thisIdx].typeIndex == other.m_componentTypes[otherIdx].typeIndex)
				thisIdx++;
			otherIdx++;
		}
		return thisIdx == m_componentTypes.size();
	}
	
	bool EntitySignature::operator==(const EntitySignature& other) const
	{
		if (m_componentTypes.size() != other.m_componentTypes.size())
			return false;
		for (size_t i = 0; i < m_componentTypes.size(); i++)
		{
			if (m_componentTypes[i].typeIndex != other.m_componentTypes[i].typeIndex)
				return false;
		}
		return true;
	}
	
	int EntitySignature::GetComponentIndex(std::type_index typeIndex) const
	{
		auto it = std::lower_bound(m_componentTypes.begin(), m_componentTypes.end(), typeIndex);
		if (it == m_componentTypes.end() || it->typeIndex != typeIndex)
			return -1;
		return it - m_componentTypes.begin();
	}
	
	bool EntitySignature::WantsMessage(std::type_index messageType) const
	{
		return std::any_of(m_componentTypes.begin(), m_componentTypes.end(), [&] (const ComponentType& component)
		{
			return component.messageReceiver != nullptr && component.messageReceiver->WantsMessage(messageType);
		});
	}
}

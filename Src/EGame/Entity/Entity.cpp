#include "Entity.hpp"
#include "EntitySignature.hpp"
#include "EntityManager.hpp"
#include "Message.hpp"

namespace eg
{
	void* Entity::GetComponentByType(std::type_index type) const
	{
		int index = m_signature->GetComponentIndex(type);
		if (index == -1)
			return nullptr;
		return GetComponentByIndex(index);
	}
	
	void* Entity::GetComponentByIndex(int index) const
	{
		if (index < (int)m_componentsDirect.size())
			return m_componentsDirect[index].Get();
		else
			return m_componentsHeap[index - m_componentsDirect.size()].Get();
	}
	
	void Entity::Initialize(uint32_t managerId, uint32_t id, const EntitySignature& signature,
		ComponentAllocator& componentAllocator, const class IEntitySerializer* serializer)
	{
		m_managerId = managerId;
		m_id = id;
		m_signature = &signature;
		m_parent = nullptr;
		m_firstChild = nullptr;
		m_queuedForDespawn = false;
		m_serializer = serializer;
		
		size_t numDirect = 0;
		for (const ComponentType& componentType : signature.ComponentTypes())
		{
			ComponentRef component = componentAllocator.Allocate(componentType);
			componentType.initializer(component.Get());
			
			if (numDirect < m_componentsDirect.size())
				m_componentsDirect[numDirect++] = component;
			else
				m_componentsHeap.push_back(component);
		}
	}
	
	void Entity::Uninitialize()
	{
		m_managerId = UINT32_MAX;
		if (m_parent != nullptr && !m_parent->m_queuedForDespawn)
		{
			if (m_prevSibling == nullptr)
			{
				m_parent->m_firstChild = m_nextSibling;
			}
			else
			{
				m_prevSibling->m_nextSibling = m_nextSibling;
			}
			
			if (m_nextSibling != nullptr)
			{
				m_nextSibling->m_prevSibling = m_prevSibling;
			}
		}
	}
	
	const Entity* Entity::FindChildBySignature(const EntitySignature& signature) const
	{
		if (signature.IsSubsetOf(*m_signature))
			return this;
		
		for (const Entity* child = m_firstChild; child != nullptr; child = child->m_nextSibling)
		{
			if (const Entity* ret = child->FindChildBySignature(signature))
				return ret;
		}
		
		return nullptr;
	}
	
	void Entity::AddChild(Entity& child)
	{
		child.m_parent = this;
		child.m_prevSibling = nullptr;
		child.m_nextSibling = m_firstChild;
		if (m_firstChild != nullptr)
		{
			m_firstChild->m_prevSibling = &child;
		}
		m_firstChild = &child;
	}
	
	void Entity::Despawn()
	{
		if (!m_queuedForDespawn)
		{
			EntityManager* manager = EntityManager::FromManagerId(m_managerId);
			manager->m_despawnQueue.emplace_back(*this);
			m_queuedForDespawn = true;
			for (Entity* child = m_firstChild; child != nullptr; child = child->m_nextSibling)
			{
				child->Despawn();
			}
		}
	}
	
	void Entity::HandleMessage(const MessageBase& message)
	{
		const auto& componentTypes = m_signature->ComponentTypes();
		for (size_t i = 0; i < componentTypes.size(); i++)
		{
			if (componentTypes[i].messageReceiver != nullptr)
			{
				componentTypes[i].messageReceiver->HandleMessage(*this, GetComponentByIndex(i), message);
			}
		}
	}
	
	Entity* EntityHandle::Get() const
	{
		if (EntityManager* manager = EntityManager::FromManagerId(m_managerId))
			return manager->FromEntityId(m_id);
		return nullptr;
	}
}

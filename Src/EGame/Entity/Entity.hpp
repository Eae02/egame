#pragma once

#include "ComponentAllocator.hpp"
#include "../API.hpp"
#include "../Utils.hpp"

#include <typeindex>
#include <iterator>

namespace eg
{
	class EG_API Entity
	{
	public:
		template <typename T>
		const T* FindComponent() const
		{
			return static_cast<const T*>(GetComponentByType(std::type_index(typeid(T))));
		}
		
		template <typename T>
		T* FindComponent()
		{
			return static_cast<T*>(GetComponentByType(std::type_index(typeid(T))));
		}
		
		template <typename T>
		const T& GetComponent() const
		{
			if (const T* comp = FindComponent<T>())
				return *comp;
			EG_PANIC("Entity does not have component " << typeid(T).name() << "!");
		}
		
		template <typename T>
		T& GetComponent()
		{
			if (T* comp = FindComponent<T>())
				return *comp;
			EG_PANIC("Entity does not have component " << typeid(T).name() << "!");
		}
		
		template <typename T, typename... A>
		T& InitComponent(A&&... args)
		{
			return GetComponent<T>() = T(std::forward<A>(args)...);
		}
		
		const class EntitySignature& Signature() const
		{
			return *m_signature;
		}
		
		const class IEntitySerializer* Serializer() const
		{
			return m_serializer;
		}
		
		void HandleMessage(const class MessageBase& message);
		
		uint32_t ManagerId() const { return m_managerId; }
		uint32_t Id() const { return m_id; }
		Entity* Parent() { return m_parent; }
		const Entity* Parent() const { return m_parent; }
		Entity* FirstChild() { return m_firstChild; }
		const Entity* FirstChild() const { return m_firstChild; }
		Entity* NextSibling() { return m_nextSibling; }
		const Entity* NextSibling() const { return m_nextSibling; }
		
		class EntityManager* Manager() const;
		
		Entity* FindChildBySignature(const class EntitySignature& signature)
		{
			return const_cast<Entity*>(const_cast<const Entity*>(this)->FindChildBySignature(signature));
		}
		
		const Entity* FindChildBySignature(const class EntitySignature& signature) const;
		
		void Despawn();
		
	private:
		friend class EntityManager;
		
		void Initialize(uint32_t managerId, uint32_t id, const EntitySignature& signature,
			ComponentAllocator& componentAllocator, const class IEntitySerializer* serializer);
		
		void Uninitialize();
		
		void AddChild(Entity& child);
		
		void* GetComponentByType(std::type_index type) const;
		
		void* GetComponentByIndex(int index) const;
		
		Entity* m_parent = nullptr;
		Entity* m_firstChild = nullptr;
		Entity* m_prevSibling = nullptr;
		Entity* m_nextSibling = nullptr;
		
		const class IEntitySerializer* m_serializer = nullptr;
		
		bool m_queuedForDespawn = false;
		uint32_t m_managerId = UINT32_MAX;
		uint32_t m_id;
		const class EntitySignature* m_signature;
		std::array<ComponentRef, 8> m_componentsDirect;
		std::vector<ComponentRef> m_componentsHeap;
	};
	
	class EG_API EntityHandle
	{
	public:
		EntityHandle()
			: m_managerId(UINT32_MAX), m_id(UINT32_MAX) { }
		
		EntityHandle(const Entity& entity)
			: m_managerId(entity.ManagerId()), m_id(entity.Id()) { }
		
		uint32_t Id() const
		{
			return m_id;
		}
		
		Entity* Get() const;
		
		template <typename T>
		T* FindComponent() const
		{
			if (Entity* entity = Get())
				return entity->FindComponent<T>();
			return nullptr;
		}
		
		bool operator==(const EntityHandle& other) const
		{
			return m_managerId == other.m_managerId && m_id == other.m_id;
		}
		
		bool operator!=(const EntityHandle& other) const
		{
			return !operator==(other);
		}
		
	private:
		uint32_t m_managerId;
		uint32_t m_id;
	};
}

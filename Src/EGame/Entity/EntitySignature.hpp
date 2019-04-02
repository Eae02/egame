#pragma once

#include "../API.hpp"
#include "../Utils.hpp"
#include "Message.hpp"

#include <type_traits>
#include <typeindex>

namespace eg
{
	struct ComponentType
	{
		std::type_index typeIndex;
		size_t size;
		size_t alignment;
		void (*initializer)(void*);
		const MessageReceiver* messageReceiver;
		
		template <typename T>
		static ComponentType Create()
		{
			static_assert(std::is_default_constructible_v<T>, "Entity components must be default constructible!");
			ComponentType res = { std::type_index(typeid(T)) };
			res.size = sizeof(T);
			res.alignment = alignof(T);
			res.initializer = [] (void* mem) { new (mem) T (); };
			if constexpr (HasMessageReceiver<T>::value)
			{
				res.messageReceiver = &T::MessageReceiver;
			}
			return res;
		}
		
		bool operator<(const ComponentType& other) const
		{
			return typeIndex < other.typeIndex;
		}
		
		bool operator<(std::type_index other) const
		{
			return typeIndex < other;
		}
	};
	
	class EG_API EntitySignature
	{
	public:
		EntitySignature() = default;
		
		template <typename... ComponentTypes>
		static EntitySignature Create()
		{
			EntitySignature signature;
			signature.InitRec<ComponentTypes...>();
			signature.EndInit();
			return signature;
		}
		
		bool IsSubsetOf(const EntitySignature& other) const;
		
		bool operator==(const EntitySignature& other) const;
		
		bool operator!=(const EntitySignature& other) const
		{
			return !operator==(other);
		}
		
		inline size_t HashCode() const
		{
			return m_hash;
		}
		
		bool WantsMessage(std::type_index messageType) const;
		
		int GetComponentIndex(std::type_index typeIndex) const;
		
		const std::vector<ComponentType>& ComponentTypes() const
		{
			return m_componentTypes;
		}
		
	private:
		template <typename T>
		static void Initializer(void* mem)
		{
			new (mem) T ();
		}
		
		template <typename T>
		void InitRec()
		{
			m_componentTypes.push_back(ComponentType::Create<T>());
		}
		
		template <typename T, typename U, typename... R>
		void InitRec()
		{
			InitRec<T>();
			InitRec<U, R...>();
		}
		
		void EndInit();
		
		std::vector<ComponentType> m_componentTypes;
		size_t m_hash;
	};
}



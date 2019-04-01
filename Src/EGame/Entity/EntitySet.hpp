#pragma once

#include "Entity.hpp"
#include "EntitySignature.hpp"

#include <iterator>

namespace eg
{
	class EG_API EntitySet
	{
	public:
		class EG_API Iterator : public std::iterator<std::forward_iterator_tag, Entity>
		{
		public:
			Iterator()
				: m_handle(nullptr), m_remaining(0), m_entity(nullptr) { }
			
			iterator& operator++()
			{
				Step();
				SkipInvalid();
				return *this;
			}
			
			iterator operator++(int)
			{
				iterator retval = *this;
				this->operator++();
				return retval;
			}
			
			bool operator==(Iterator other) const
			{
				return (m_remaining == 0 && other.m_remaining == 0) || m_handle == other.m_handle;
			}
			
			bool operator!=(Iterator other) const
			{
				return !operator==(other);
			}
			
			reference operator*() const
			{
				return *m_entity;
			}
			
		private:
			friend class EntitySet;
			
			const EntityHandle* m_handle;
			size_t m_remaining;
			Entity* m_entity;
			
			void Step()
			{
				if (m_remaining > 0)
				{
					m_handle++;
					m_remaining--;
				}
			}
			
			void SkipInvalid()
			{
				while (m_remaining > 0 && (m_entity = m_handle->Get()) == nullptr)
					Step();
			}
			
			explicit Iterator(const std::vector<EntityHandle>& handles)
				: m_handle(handles.data()), m_remaining(handles.size())
			{
				SkipInvalid();
			}
		};
		
		explicit EntitySet(const class EntitySignature& signature)
			: m_signature(&signature) { }
		
		const class EntitySignature& Signature() const
		{
			return *m_signature;
		}
		
		Iterator begin() const
		{
			return Iterator(m_entities);
		}
		
		Iterator end() const
		{
			return Iterator();
		}
		
		void RemoveDead();
		
		void MaybeAdd(const Entity& entity);
		
	private:
		const class EntitySignature* m_signature;
		std::vector<EntityHandle> m_entities;
	};
}

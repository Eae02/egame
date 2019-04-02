#pragma once

#include <typeindex>

#include "../API.hpp"

namespace eg
{
	class MessageBase
	{
	public:
		std::type_index GetType() const
		{
			return m_type;
		}
		
	protected:
		explicit MessageBase(std::type_index type)
			: m_type(type) { }
		
	private:
		std::type_index m_type;
	};
	
	template <typename T>
	class Message : public MessageBase
	{
	public:
		Message()
			: MessageBase(std::type_index(typeid(T))) { }
	};
	
	struct MessageHandler
	{
		std::type_index messageType;
		void (*callback)(class Entity& entity, void* component, const MessageBase& message);
	};
	
	class EG_API MessageReceiver
	{
	public:
		bool WantsMessage(std::type_index type) const;
		
		bool HandleMessage(class Entity& entity, void* component, const MessageBase& message) const;
		
		std::type_index HandlerType() const
		{
			return m_handlerType;
		}
		
		const std::vector<MessageHandler>& MessageHandlers() const
		{
			return m_handlers;
		}
		
		template <typename HandlerType, typename... MessageTypes>
		static MessageReceiver Create()
		{
			MessageReceiver receiver(std::type_index(typeid(HandlerType)), sizeof...(MessageTypes));
			receiver.InitHandlers<HandlerType, MessageTypes...>();
			receiver.SortHandlers();
			return receiver;
		}
		
	private:
		void SortHandlers();
		
		template <typename HandlerType, typename M>
		void InitHandlers()
		{
			MessageHandler& handler = m_handlers.emplace_back(MessageHandler { std::type_index(typeid(M)) });
			handler.callback = [] (class Entity& entity, void* component, const MessageBase& message)
			{
				static_cast<HandlerType*>(component)->HandleMessage(entity, static_cast<const M&>(message));
			};
		}
		
		template <typename HandlerType, typename M1, typename M2, typename... MR>
		void InitHandlers()
		{
			InitHandlers<HandlerType, M1>();
			InitHandlers<HandlerType, M2, MR...>();
		}
		
		MessageReceiver(std::type_index handlerType, size_t numHandlers)
			: m_handlerType(handlerType)
		{
			m_handlers.reserve(numHandlers);
		}
		
		std::type_index m_handlerType;
		std::vector<MessageHandler> m_handlers;
	};
	
	template <typename T, typename = int>
	struct HasMessageReceiver : std::false_type { };
	
	template <typename T>
	struct HasMessageReceiver <T, decltype((void)T::MessageReceiver, 0)> : std::true_type { };
}

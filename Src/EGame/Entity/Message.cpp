#include "Message.hpp"

namespace eg
{
	struct MessageHandlerCompare
	{
		bool operator()(const MessageHandler& a, const MessageHandler& b) const
		{
			return a.messageType < b.messageType;
		}
		
		bool operator()(const MessageHandler& a, std::type_index b) const
		{
			return a.messageType < b;
		}
	};
	
	bool MessageReceiver::WantsMessage(std::type_index type) const
	{
		auto it = std::lower_bound(m_handlers.begin(), m_handlers.end(), type, MessageHandlerCompare());
		return it != m_handlers.end() && it->messageType == type;
	}
	
	bool MessageReceiver::HandleMessage(class Entity& entity, void* component, const MessageBase& message) const
	{
		auto it = std::lower_bound(m_handlers.begin(), m_handlers.end(), message.GetType(), MessageHandlerCompare());
		if (it == m_handlers.end() || it->messageType != message.GetType())
			return false;
		it->callback(entity, component, message);
		return true;
	}
	
	void MessageReceiver::SortHandlers()
	{
		std::sort(m_handlers.begin(), m_handlers.end(), MessageHandlerCompare());
	}
}

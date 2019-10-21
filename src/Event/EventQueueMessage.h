#pragma once

#include "common/basic_types.h"
#include "common/ref_count.h"

namespace TB8
{

// message id type.
enum EventMessageID : u32;

// base message class for messages.
class EventMessage : public ref_count
{
public:
	EventMessage(EventMessageID messageID)
		: m_messageID(messageID)
		, m_objectID(0)
	{
	}

	EventMessage(EventMessageID messageID, u32 objectID)
		: m_messageID(messageID)
		, m_objectID(objectID)
	{
	}

	EventMessageID GetID() const { return m_messageID; }
	u32 GetObjectID() const { return m_objectID; }

private:
	virtual void __free() = 0;

	EventMessageID		m_messageID;
	u32					m_objectID;
};

}

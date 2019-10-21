#pragma once

#include <map>
#include <set>
#include <deque>
#include <functional>

#include "common/basic_types.h"

#include "EventQueueModules.h"
#include "EventQueueMessage.h"

namespace TB8
{

typedef std::function<void(EventMessage*)> EventQueueCallback;

struct EventQueueMessageTarget
{
	u32					m_objectID;
	EventMessageID		m_messageID;

	bool operator <(const EventQueueMessageTarget& rhs) const;
};

struct EventQueueRegistration
{
	EventModuleID		m_moduleID;
	u32					m_subModuleID;
	EventQueueCallback	m_pCallback;

	bool operator <(const EventQueueRegistration& rhs) const;
};

struct EventQueueDeferredUnregistration
{
	u32					m_objectID;
	EventMessageID		m_messageID;
	EventModuleID		m_moduleID;
	u32					m_subModuleID;
};

struct EventQueuePending
{
	std::deque<EventMessage*>				m_messages;

	~EventQueuePending();
};

class EventQueue
{
public:
	EventQueue();
	~EventQueue();

	static EventQueue* Alloc();
	void Free();

	void QueueMessage(EventMessage** message);
	void ProcessPending();

	void RegisterForMessage(EventModuleID moduleID, EventMessageID messageID, EventQueueCallback cb);
	void RegisterForMessage(EventModuleID moduleID, u32 subModuleID, EventMessageID targetMessageID, u32 targetObjectID, EventQueueCallback cb);

	void UnregisterForMessage(EventModuleID moduleID, u32 subModuleID, EventMessageID targetMessageID, u32 targetObjectID);

	void UnregisterForMessagesByRegistree(EventModuleID moduleID);
	void UnregisterForMessagesByRegistree(EventModuleID moduleID, u32 subModuleID);

	void UnregisterForMessagesByTargetObjectID(u32 objectID);

private:
	void __DispatchMessageObject(EventMessage* message);
	void __DispatchMessageGeneric(EventMessage* message);
	void __DispatchMessage(EventMessage* message, std::set<EventQueueRegistration>& registrations);

	void __CleanupTargetObjectIDs();
	void __CleanupDestroyedTargets();
	void __ProcessDeferredUnregistrations();

	u32 __GetNextIndex() const { return m_index ? 1 : 0; }

	u32								m_index;
	EventQueuePending				m_pending[2];

	std::map<EventQueueMessageTarget, std::set<EventQueueRegistration>> m_targets;

	std::deque<u32>									m_destroyedTargets;
	std::deque<EventQueueDeferredUnregistration>	m_deferredUnregistrations;
};

}


/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/
#include "pch.h"

#include "EventQueueModules.h"
#include "EventQueueMessage.h"
#include "EventQueueMessages.h"

#include "EventQueue.h"

namespace TB8
{

bool EventQueueRegistration::operator <(const EventQueueRegistration& rhs) const
{
	if (m_moduleID != rhs.m_moduleID)
		return m_moduleID < rhs.m_moduleID;
	return m_subModuleID < rhs.m_subModuleID;
}

bool EventQueueMessageTarget::operator <(const EventQueueMessageTarget& rhs) const
{
	if (m_objectID != rhs.m_objectID)
		return m_objectID < rhs.m_objectID;
	return m_messageID < rhs.m_messageID;
}

EventQueuePending::~EventQueuePending()
{
	while (!m_messages.empty())
	{
		EventMessage* message = m_messages.front();
		m_messages.pop_front();
		message->Release();
	}
}

EventQueue::EventQueue()
	: m_index(0)
{
}

EventQueue::~EventQueue()
{
}

/* static */ EventQueue* EventQueue::Alloc()
{
	EventQueue* obj = TB8_NEW(EventQueue)();
	return obj;
}

void EventQueue::Free()
{
	TB8_DEL(this);
}

void EventQueue::QueueMessage(EventMessage** message)
{
	// steals ownership of the reference to the message.
	m_pending[m_index].m_messages.push_back(*message);
	*message = nullptr;
}

void EventQueue::ProcessPending()
{
	// swap the queue.
	EventQueuePending& pending = m_pending[m_index];
	m_index = __GetNextIndex();

	// process pending messages.
	while (!pending.m_messages.empty())
	{
		// get the first message.
		EventMessage* message = pending.m_messages.front();
		pending.m_messages.pop_front();

		// dispatch to those that specifically asked for this object id.
		__DispatchMessageObject(message);

		// dispatch to those that wanted this message, and didn't specify an object id.
		__DispatchMessageGeneric(message);

		// we're done with this message, release it.
		message->Release();
	}

	// process targets & registrees that unregistered.
	__CleanupDestroyedTargets();
	__ProcessDeferredUnregistrations();
}

void EventQueue::__CleanupDestroyedTargets()
{
	while (!m_destroyedTargets.empty())
	{
		EventQueueMessageTarget target;
		target.m_objectID = m_destroyedTargets.front();
		target.m_messageID = EventMessageID_Invalid;

		std::map<EventQueueMessageTarget, std::set<EventQueueRegistration>>::iterator itTarget = m_targets.lower_bound(target);
		while ((itTarget != m_targets.end()) && (itTarget->first.m_objectID == target.m_objectID))
		{
			std::map<EventQueueMessageTarget, std::set<EventQueueRegistration>>::iterator itDel = itTarget;
			++itTarget;
			m_targets.erase(itDel);
		}

		m_destroyedTargets.pop_front();
	}
}

void EventQueue::__ProcessDeferredUnregistrations()
{
	while (!m_deferredUnregistrations.empty())
	{
		const EventQueueDeferredUnregistration& unreg = m_deferredUnregistrations.front();

		EventQueueMessageTarget target;
		target.m_objectID = unreg.m_objectID;
		target.m_messageID = unreg.m_messageID;

		std::map<EventQueueMessageTarget, std::set<EventQueueRegistration>>::iterator itTarget = m_targets.find(target);
		if (itTarget != m_targets.end())
		{
			std::set<EventQueueRegistration>& registrations = itTarget->second;

			EventQueueRegistration reg;
			reg.m_moduleID = unreg.m_moduleID;
			reg.m_subModuleID = unreg.m_subModuleID;

			registrations.erase(reg);
		}

		m_deferredUnregistrations.pop_front();
	}
}

void EventQueue::__DispatchMessageObject(EventMessage* message)
{
	if (message->GetObjectID() == 0)
		return;

	EventQueueMessageTarget target;
	target.m_objectID = message->GetObjectID();
	target.m_messageID = message->GetID();

	// are there registrations for this message?
	std::map<EventQueueMessageTarget, std::set<EventQueueRegistration>>::iterator itTarget = m_targets.find(target);
	if (itTarget == m_targets.end())
		return;

	// go thru all the registrations, call everyone.
	__DispatchMessage(message, itTarget->second);
}

void EventQueue::__DispatchMessageGeneric(EventMessage* message)
{
	EventQueueMessageTarget target;
	target.m_objectID = 0;
	target.m_messageID = message->GetID();

	// are there registrations for this message?
	std::map<EventQueueMessageTarget, std::set<EventQueueRegistration>>::iterator itTarget = m_targets.find(target);
	if (itTarget == m_targets.end())
		return;

	// go thru all the registrations, call everyone.
	__DispatchMessage(message, itTarget->second);
}

void EventQueue::__DispatchMessage(EventMessage* message, std::set<EventQueueRegistration>& registrations)
{
	// go thru all the registrations, call everyone.
	for (std::set<EventQueueRegistration>::iterator itReg = registrations.begin(); itReg != registrations.end(); ++itReg)
	{
		const EventQueueRegistration& reg = *itReg;
		if (reg.m_pCallback)
		{
			reg.m_pCallback(message);
		}
	}
}

void EventQueue::RegisterForMessage(EventModuleID moduleID, EventMessageID messageID, EventQueueCallback cb)
{
	RegisterForMessage(moduleID, 0, messageID, 0, cb);
}

void EventQueue::RegisterForMessage(EventModuleID moduleID, u32 subModuleID, EventMessageID messageID, u32 objectID, EventQueueCallback cb)
{
	EventQueueMessageTarget target;
	target.m_objectID = objectID;
	target.m_messageID = messageID;

	EventQueueRegistration reg;
	reg.m_moduleID = moduleID;
	reg.m_subModuleID = subModuleID;
	reg.m_pCallback = cb;

	// look up the event.
	std::map<EventQueueMessageTarget, std::set<EventQueueRegistration>>::iterator itTarget = m_targets.find(target);
	if (itTarget == m_targets.end())
	{
		itTarget = m_targets.insert(std::make_pair(target, std::set<EventQueueRegistration>())).first;
	}
	std::set<EventQueueRegistration>& registrations = itTarget->second;

	// add the registration
	std::pair<std::set<EventQueueRegistration>::iterator, bool> p = registrations.insert(reg);
	assert(p.second); // fixme: deal with re-registering.
}

void EventQueue::UnregisterForMessage(EventModuleID moduleID, u32 subModuleID, EventMessageID targetMessageID, u32 targetObjectID)
{
	EventQueueMessageTarget target;
	target.m_objectID = targetObjectID;
	target.m_messageID = targetMessageID;

	EventQueueRegistration regFind;
	regFind.m_moduleID = moduleID;
	regFind.m_subModuleID = subModuleID;
	regFind.m_pCallback = nullptr;

	// look up the event.
	std::map<EventQueueMessageTarget, std::set<EventQueueRegistration>>::iterator itTarget = m_targets.find(target);
	if (itTarget == m_targets.end())
		return;
	std::set<EventQueueRegistration>& registrations = itTarget->second;

	// look up the registration.
	std::set<EventQueueRegistration>::iterator itReg = registrations.find(regFind);
	if (itReg == registrations.end())
		return;

	EventQueueRegistration& reg = const_cast<EventQueueRegistration&>(*itReg);
	reg.m_pCallback = nullptr;

	EventQueueDeferredUnregistration unreg;
	unreg.m_objectID = targetObjectID;
	unreg.m_messageID = targetMessageID;
	unreg.m_moduleID = moduleID;
	unreg.m_subModuleID = subModuleID;
	m_deferredUnregistrations.push_back(unreg);
}

void EventQueue::UnregisterForMessagesByRegistree(EventModuleID moduleID)
{
	UnregisterForMessagesByRegistree(moduleID, 0);
}

void EventQueue::UnregisterForMessagesByRegistree(EventModuleID moduleID, u32 subModuleID)
{
	EventQueueRegistration regFind;
	regFind.m_moduleID = moduleID;
	regFind.m_subModuleID = subModuleID;
	regFind.m_pCallback = nullptr;

	for (std::map<EventQueueMessageTarget, std::set<EventQueueRegistration>>::iterator itTarget = m_targets.begin();
		itTarget != m_targets.end(); ++itTarget)
	{
		const EventQueueMessageTarget& target = itTarget->first;
		std::set<EventQueueRegistration>& registrations = itTarget->second;
		std::set<EventQueueRegistration>::iterator itReg = registrations.find(regFind);
		if (itReg != registrations.end())
		{
			EventQueueRegistration& reg = const_cast<EventQueueRegistration&>(*itReg);
			reg.m_pCallback = nullptr;

			EventQueueDeferredUnregistration unreg;
			unreg.m_objectID = target.m_objectID;
			unreg.m_messageID = target.m_messageID;
			unreg.m_moduleID = moduleID;
			unreg.m_subModuleID = subModuleID;
			m_deferredUnregistrations.push_back(unreg);
		}
	}
}

void EventQueue::UnregisterForMessagesByTargetObjectID(u32 objectID)
{
	m_destroyedTargets.push_back(objectID);
}

}

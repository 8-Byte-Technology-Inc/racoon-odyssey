/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/
#include "pch.h"

#include "Event/EventQueueMessage.h"
#include "Event/EventQueue.h"

#include "Client_Globals.h"

// Client_Globals
Client_Globals::Client_Globals(TB8::EventQueue* pEventQueue)
	: m_pRenderer(nullptr)
	, m_isShutdown(false)
	, m_pEventQueue(pEventQueue)
	, m_currentFrameCount(0)
	, m_idGen(0)
{
}

Client_Globals::~Client_Globals()
{
	m_pRenderer = nullptr;
	m_pEventQueue = nullptr;
}

Client_Globals* Client_Globals::Alloc(TB8::EventQueue* pEventQueue)
{
	Client_Globals* pObj = TB8_NEW(Client_Globals)(pEventQueue);
	return pObj;
}

void Client_Globals::Free()
{
	TB8_DEL(this);
}

void Client_Globals::Initialize(TB8::RenderMain* pRenderer)
{
	// set renderer.
	m_pRenderer = pRenderer;
}

void Client_Globals::QueueEvent(TB8::EventMessage** ppMessage)
{
	if (m_isShutdown)
	{
		RELEASEI(*ppMessage);
	}
	else
	{
		m_pEventQueue->QueueMessage(ppMessage);
	}
}

void Client_Globals::Shutdown()
{
	m_isShutdown = true;
}

// Client_Globals_Accessor
Client_Globals_Accessor::Client_Globals_Accessor()
	: m_pGlobals(nullptr)
{
}

Client_Globals_Accessor::Client_Globals_Accessor(Client_Globals* pGlobals)
	: m_pGlobals(pGlobals)
{
}

Client_Globals_Accessor::Client_Globals_Accessor(const Client_Globals_Accessor& src)
	: m_pGlobals(nullptr)
{
}

Client_Globals_Accessor::Client_Globals_Accessor(const Client_Globals_Accessor&& src)
	: m_pGlobals(src.m_pGlobals)
{
}

Client_Globals_Accessor::~Client_Globals_Accessor()
{
}

void Client_Globals_Accessor::__SetGlobals(Client_Globals* pGlobalState)
{
	m_pGlobals = pGlobalState;
}

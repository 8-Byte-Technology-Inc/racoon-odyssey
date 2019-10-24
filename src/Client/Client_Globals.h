#pragma once

#include "common/basic_types.h"
#include "common/memory.h"

namespace TB8
{
	class EventQueue;
	class EventMessage;
	class RenderMain;
}

class Client_Globals
{
public:
	Client_Globals(TB8::EventQueue* pEventQueue);
	~Client_Globals();
	static Client_Globals* Alloc(TB8::EventQueue* pEventQueue);
	void Free();
	void Initialize(TB8::RenderMain* pRenderer);

	TB8::RenderMain* GetRenderer() { return m_pRenderer; }
	const TB8::RenderMain* GetRenderer() const { return m_pRenderer; }
	void QueueEvent(TB8::EventMessage** ppMessage);
	TB8::EventQueue* GetEventQueue() { return m_pEventQueue; }
	u32 GetCurrentFrameCount() const { return m_currentFrameCount; }
	void UpdateFrameCount(s32 frameCount) { m_currentFrameCount += static_cast<u32>(frameCount); }
	u32 GetNewID() { return ++m_idGen; }
	void Shutdown();
	void SetPathAssets(const std::string& pathAssets) { m_pathAssets = pathAssets; }
	void SetPathBinary(const std::string& pathBinary) { m_pathBinary = pathBinary; }
	const std::string& GetPathAssets() const { return m_pathAssets; }
	const std::string& GetPathBinary() const { return m_pathBinary; }

private:
	TB8::RenderMain*								m_pRenderer;				// renderer.

	bool											m_isShutdown;				// true if we're shutting down.

	TB8::EventQueue*								m_pEventQueue;				// queue events

	u32												m_currentFrameCount;		// current frame count.

	u32												m_idGen;					// id generation.

	std::string										m_pathAssets;				// path to assets.
	std::string										m_pathBinary;				// path to binary.
};

class Client_Globals_Accessor
{
public:
	Client_Globals_Accessor();
	Client_Globals_Accessor(Client_Globals* pGlobalState);
	Client_Globals_Accessor(const Client_Globals_Accessor& src);
	Client_Globals_Accessor(const Client_Globals_Accessor&& src);
	~Client_Globals_Accessor();

protected:
	void __SetGlobals(Client_Globals* pGlobalState);

	TB8::RenderMain* __GetRenderer() { return m_pGlobals->GetRenderer(); }
	const TB8::RenderMain* __GetRenderer() const { return m_pGlobals->GetRenderer(); }
	Client_Globals* __GetGlobals() { return m_pGlobals; }
	const Client_Globals* __GetGlobals() const { return m_pGlobals; }
	u32 __GetCurrentFrameCount() const { return m_pGlobals->GetCurrentFrameCount(); }
	void __UpdateFrameCount(u32 frameCount) { m_pGlobals->UpdateFrameCount(frameCount); }
	void __QueueEvent(TB8::EventMessage** ppMessage) { m_pGlobals->QueueEvent(ppMessage); }
	TB8::EventQueue* __GetEventQueue() { return m_pGlobals->GetEventQueue(); }
	u32 __GetNewID() { return m_pGlobals->GetNewID(); }
	const std::string& __GetPathAssets() const { return m_pGlobals->GetPathAssets(); }
	const std::string& __GetPathBinary() const { return m_pGlobals->GetPathBinary(); }

private:
	Client_Globals*									m_pGlobals;
};

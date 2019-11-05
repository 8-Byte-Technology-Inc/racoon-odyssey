#pragma once

#include "common/basic_types.h"

namespace TB8
{
	class RenderMain;
	class World;
	class EventQueue;
}

class Client_Globals;

class MainClass
{
public:
	MainClass();
	~MainClass();

	HWND GetWindowHandle() { return m_hWnd; };

	void Initialize();
	void Run();

private:

	void __Shutdown();

	HRESULT __CreateDesktopWindow();

	void __ProcessRenderFrame();
	void __ProcessRenderFrame(s32 frameCount);
	void __DispatchEventQueue();
	void __ProcessInternal();

	static LRESULT CALLBACK __StaticWindowProc(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam
	);
	LRESULT CALLBACK __WindowProc(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam
	);

	HINSTANCE			m_hInstance;
	RECT				m_rc;
	HWND				m_hWnd;
	bool				m_inSizeMove;
	bool				m_isTrackingMouse;

	IVector2			m_lastMousePos;
	bool				m_isMouseButtonDown;
	u32					m_mouseButtonDownFrameCount;

	s64					m_clockTickPerSecond;			// counts / second.
	s64					m_clockTickPerMilliSecond;		// counts / millisecond.
	s64					m_clockTickPerRenderFrame;		// counts / live frame.
	s64					m_clockTickPerComputeFrame;		// counts / master frame.

	s64					m_clockTickLastRenderFrame;		// tick count last live frame.
	s64					m_clockTickLastComputeFrame;	// tick count last master frame.

	Client_Globals*		m_pClientGlobals;

	TB8::World*			m_pWorld;

	TB8::RenderMain*	m_pRenderer;

	TB8::EventQueue*	m_eventQueue;
};

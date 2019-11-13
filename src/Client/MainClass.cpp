/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/
#include "pch.h"

#include <ShellScalingApi.h>
#include <winuser.h>

#include "common/file_io.h"

#include "resource.h"

#include "render/RenderMain.h"
#include "render/RenderModel.h"
#include "render/RenderHelper.h"

#include "event/EventQueue.h"
#include "event/EventQueueMessages.h"

#include "world/World.h"

#include "Client_Globals.h"
#include "MainClass.h"

const WCHAR* s_windowClassName = L"Racoon-Odyssey-Class";

// MainClass.
MainClass::MainClass()
	: m_eventQueue(nullptr)
	, m_pClientGlobals(nullptr)
	, m_pWorld(nullptr)
	, m_pRenderer(nullptr)
	, m_hInstance(NULL)
	, m_hWnd(NULL)
	, m_inSizeMove(false)
	, m_isTrackingMouse(false)
	, m_isMouseButtonDown(false)
	, m_mouseButtonDownFrameCount(0)
{
	m_hInstance = (HINSTANCE)GetModuleHandle(NULL);
	LARGE_INTEGER val;
	QueryPerformanceFrequency(&val);
	m_clockTickPerSecond = val.QuadPart;
	m_clockTickPerMilliSecond = m_clockTickPerSecond / 1000LL;
	m_clockTickPerRenderFrame = m_clockTickPerSecond / 60LL;
	m_clockTickPerComputeFrame = m_clockTickPerSecond / 60LL;

	QueryPerformanceCounter(&val);
	m_clockTickLastRenderFrame = val.QuadPart;
	m_clockTickLastComputeFrame = val.QuadPart;
}

HRESULT MainClass::__CreateDesktopWindow()
{
	WCHAR szExePath[MAX_PATH];
	GetModuleFileName(NULL, szExePath, MAX_PATH);

	HICON hIcon = ExtractIcon(m_hInstance, szExePath, 0);

	// Register the windows class
	WNDCLASS wndClass;
	wndClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = MainClass::__StaticWindowProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = sizeof(MainClass*);
	wndClass.hInstance = m_hInstance;
	wndClass.hIcon = hIcon;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = s_windowClassName;

	if (!RegisterClass(&wndClass))
	{
		DWORD dwError = GetLastError();
		if (dwError != ERROR_CLASS_ALREADY_EXISTS)
			return HRESULT_FROM_WIN32(dwError);
	}

	m_rc;
	int x = CW_USEDEFAULT;
	int y = CW_USEDEFAULT;

	int nDefaultWidth = 640;
	int nDefaultHeight = 480;
	SetRect(&m_rc, 0, 0, nDefaultWidth, nDefaultHeight);
	AdjustWindowRect(
		&m_rc,
		WS_OVERLAPPEDWINDOW,
		false
	);

	WCHAR szTitle[128];
	int r = LoadString(m_hInstance, IDS_TITLE, szTitle, ARRAYSIZE(szTitle));
	assert(r > 0);

	m_hWnd = CreateWindow(
		s_windowClassName,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		x, y,
		(m_rc.right - m_rc.left), (m_rc.bottom - m_rc.top),
		0,
		NULL,
		m_hInstance,
		0
	);

	SetWindowLongPtr(m_hWnd, 0, reinterpret_cast<LONG_PTR>(this));

	if (m_hWnd == NULL)
	{
		DWORD dwError = GetLastError();
		return HRESULT_FROM_WIN32(dwError);
	}

	return S_OK;
}

void MainClass::Initialize()
{
	// set dpi awareness.
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	// create event queue.
	m_eventQueue = TB8::EventQueue::Alloc();

	// create globals
	m_pClientGlobals = Client_Globals::Alloc(m_eventQueue);

	// set binary & asset paths.
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, ARRAYSIZE(path));

	std::string pathBinary = path;
	TB8::File::StripFileNameFromPath(pathBinary);
	m_pClientGlobals->SetPathBinary(pathBinary);

	std::string pathAssets = pathBinary;
	TB8::File::AppendToPath(pathAssets, "../../../../../src/Assets");
	m_pClientGlobals->SetPathAssets(pathAssets);

	// create the desktop window.
	__CreateDesktopWindow();

	// create renderer
	m_pRenderer = TB8::RenderMain::Alloc(m_hWnd, m_pClientGlobals);

	// initialize globals.
	m_pClientGlobals->Initialize(m_pRenderer);

	// create world.
	m_pWorld = TB8::World::Alloc(m_pClientGlobals);
	m_pWorld->LoadMap("maps/wall-maze/wall-maze.xml");
	m_pWorld->LoadCharacter("mooey/mooey.dae", "Mooey");
}

void MainClass::Run()
{
	if (!IsWindowVisible(m_hWnd))
		ShowWindow(m_hWnd, SW_SHOW);

	bool bGotMsg;
	MSG  msg;
	msg.message = WM_NULL;
	PeekMessage(&msg, NULL, 0U, 0U, PM_NOREMOVE);

	while (WM_QUIT != msg.message)
	{
		bGotMsg = (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE) != 0);

		if (bGotMsg)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!bGotMsg)
		{
			// dispatch event queue.
			__DispatchEventQueue();

			// process render frame.
			__ProcessRenderFrame();
		}
	}
}

void MainClass::__ProcessRenderFrame()
{
	// process render frame.
	LARGE_INTEGER clockTickCurrent;
	QueryPerformanceCounter(&clockTickCurrent);
	s64 ticksSinceLastLiveFrame = clockTickCurrent.QuadPart - m_clockTickLastRenderFrame;
	if (ticksSinceLastLiveFrame < m_clockTickPerRenderFrame)
		return;

	s32 frameCount = static_cast<s32>(ticksSinceLastLiveFrame / m_clockTickPerRenderFrame);
	m_clockTickLastRenderFrame += (frameCount * m_clockTickPerRenderFrame);

	__ProcessRenderFrame(frameCount);
}

void MainClass::__ProcessRenderFrame(s32 frameCount)
{
	// update frame count on globals.
	m_pClientGlobals->UpdateFrameCount(frameCount);

	__ProcessInternal();

	m_pRenderer->UpdateRender(frameCount);
	m_pWorld->Update(frameCount);

	m_pRenderer->BeginUpdate();

	m_pWorld->Render3D(m_pRenderer);

	m_pRenderer->BeginDraw();

	m_pWorld->Render2D(m_pRenderer);

	m_pRenderer->DrawCursor();

	m_pRenderer->EndDraw();

	m_pRenderer->EndUpdate();
}

void MainClass::__DispatchEventQueue()
{
	// update any queued messages to registered systems.
	m_eventQueue->ProcessPending();
}

void MainClass::__ProcessInternal()
{
	// if the mouse button is down, periodically fire events to indicate the button is held down.
	if (m_isMouseButtonDown)
	{
		u32 frameCount = m_pClientGlobals->GetCurrentFrameCount() - m_mouseButtonDownFrameCount;
		TB8::EventMessage* event = TB8::EventMessage_MouseLBDownHold::Alloc(frameCount, m_lastMousePos);
		m_eventQueue->QueueMessage(&event);
	}
}

MainClass::~MainClass()
{
	__Shutdown();
}

void MainClass::__Shutdown()
{
	m_pClientGlobals->Shutdown();

	OBJFREE(m_pWorld);
	OBJFREE(m_pRenderer);
	OBJFREE(m_pClientGlobals);
	OBJFREE(m_eventQueue);
}

LRESULT CALLBACK MainClass::__StaticWindowProc(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
)
{
	MainClass* pMain = reinterpret_cast<MainClass*>(GetWindowLongPtr(hWnd, 0));
	return pMain->__WindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MainClass::__WindowProc(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
)
{
	switch (uMsg)
	{
	case WM_CLOSE:
	{
		HMENU hMenu;
		hMenu = GetMenu(m_hWnd);
		if (hMenu != NULL)
		{
			DestroyMenu(hMenu);
		}
		DestroyWindow(hWnd);
		UnregisterClass(
			s_windowClassName,
			m_hInstance
		);
		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_KEYDOWN:
		if ((wParam == 'W') || (wParam == VK_UP))
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveStart::Alloc(+0, -1, +0);
			m_eventQueue->QueueMessage(&event);
		}
		else if ((wParam == 'A') || (wParam == VK_LEFT))
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveStart::Alloc(-1, +0, +0);
			m_eventQueue->QueueMessage(&event);
		}
		else if ((wParam == 'S') || (wParam == VK_DOWN))
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveStart::Alloc(+0, +1, +0);
			m_eventQueue->QueueMessage(&event);
		}
		else if ((wParam == 'D') || (wParam == VK_RIGHT))
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveStart::Alloc(+1, +0, +0);
			m_eventQueue->QueueMessage(&event);
		}
		else if (wParam == VK_SPACE)
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveStart::Alloc(+0, +0, +1);
			m_eventQueue->QueueMessage(&event);
		}

		break;

	case WM_KEYUP:
		if ((wParam == 'W') || (wParam == VK_UP))
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveEnd::Alloc(+0, -1, +0);
			m_eventQueue->QueueMessage(&event);
		}
		else if ((wParam == 'A') || (wParam == VK_LEFT))
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveEnd::Alloc(-1, +0, +0);
			m_eventQueue->QueueMessage(&event);
		}
		else if ((wParam == 'S') || (wParam == VK_DOWN))
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveEnd::Alloc(+0, +1, +0);
			m_eventQueue->QueueMessage(&event);
		}
		else if ((wParam == 'D') || (wParam == VK_RIGHT))
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveEnd::Alloc(+1, +0, +0);
			m_eventQueue->QueueMessage(&event);
		}
		else if (wParam == VK_SPACE)
		{
			TB8::EventMessage* event = TB8::EventMessage_MoveStart::Alloc(+0, +0, -1);
			m_eventQueue->QueueMessage(&event);
		}
		else if (wParam == 'E')
		{
			TB8::EventMessage* event = TB8::EventMessage_Activate::Alloc();
			m_eventQueue->QueueMessage(&event);
		}
		else if (wParam == 'R')
		{
			TB8::EventMessage* event = TB8::EventMessage_Rotate::Alloc();
			m_eventQueue->QueueMessage(&event);
		}
		else if (wParam == VK_ESCAPE)
		{
			TB8::EventMessage* event = TB8::EventMessage_Close::Alloc();
			m_eventQueue->QueueMessage(&event);
		}

		break;

	case WM_MOUSEWHEEL:
	{
		s16 mouseX = static_cast<s16>(GET_X_LPARAM(lParam));
		s16 mouseY = static_cast<s16>(GET_Y_LPARAM(lParam));

		POINT point = { mouseX, mouseY };
		ScreenToClient(m_hWnd, &point);

		const IVector2 mousePos(point.x, point.y);
		m_lastMousePos = mousePos;

		const s16 delta = static_cast<s16>(HIWORD(wParam));

		TB8::EventMessage* event = TB8::EventMessage_MouseMoveWheel::Alloc(mousePos, delta, WHEEL_DELTA);
		m_eventQueue->QueueMessage(&event);
		break;
	}
	case WM_MOUSEMOVE:
	{
		s16 mouseX = GET_X_LPARAM(lParam);
		s16 mouseY = GET_Y_LPARAM(lParam);

		m_pRenderer->SetRenderMousePos(IVector2(mouseX, mouseY));

		// if we aren't already, start tracking the mouse.
		if (!m_isTrackingMouse)
		{
			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_HOVER | TME_LEAVE;
			tme.hwndTrack = m_hWnd;
			tme.dwHoverTime = HOVER_DEFAULT;
			TrackMouseEvent(&tme);
			m_isTrackingMouse = true;
		}

		m_lastMousePos = IVector2(mouseX, mouseY);

		TB8::EventMessage* event = TB8::EventMessage_MouseCursorMove::Alloc(mouseX, mouseY);
		m_eventQueue->QueueMessage(&event);
		break;
	}
	case WM_MOUSEHOVER:
	{
		// the mouse is hovering in one spot.
		IVector2 mousePos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

		m_lastMousePos = mousePos;

		TB8::EventMessage* event = TB8::EventMessage_MouseHover::Alloc(mousePos);
		m_eventQueue->QueueMessage(&event);
		break;
	}
	case WM_MOUSELEAVE:
	{
		// if the mouse leaves, the tracking is automatically turned off.
		m_isTrackingMouse = false;

		// which edge were we closest to?
		const IVector2& screenSize = m_pRenderer->GetRenderScreenSize();
		IVector2 mousePos = m_pRenderer->GetRenderMousePos();

		const s32 x1 = std::abs(mousePos.x);
		const s32 x2 = std::abs(screenSize.x - mousePos.x);

		const s32 y1 = std::abs(mousePos.y);
		const s32 y2 = std::abs(screenSize.y - mousePos.y);

		if (x1 < x2 && x1 < y1 && x1 < y2)
		{
			mousePos.x = -1;
		}
		else if (x2 < x1 && x2 < y1 && x2 < y2)
		{
			mousePos.x = screenSize.x + 1;
		}
		else if (y1 < x1 && y1 < x2 && y1 < y2)
		{
			mousePos.y = -1;
		}
		else
		{
			mousePos.y = screenSize.y + 1;
		}

		m_pRenderer->SetRenderMousePos(mousePos);
		m_lastMousePos = mousePos;

		TB8::EventMessage* event = TB8::EventMessage_MouseLeave::Alloc(mousePos);
		m_eventQueue->QueueMessage(&event);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		s16 mouseX = GET_X_LPARAM(lParam);
		s16 mouseY = GET_Y_LPARAM(lParam);

		TB8::EventMessage* event = TB8::EventMessage_MouseLBDown::Alloc(mouseX, mouseY);
		m_eventQueue->QueueMessage(&event);
		SetCapture(m_hWnd);

		m_lastMousePos = IVector2(mouseX, mouseY);

		m_isMouseButtonDown = true;
		m_mouseButtonDownFrameCount = m_pClientGlobals->GetCurrentFrameCount();
		break;
	}
	case WM_LBUTTONUP:
	{
		s16 mouseX = GET_X_LPARAM(lParam);
		s16 mouseY = GET_Y_LPARAM(lParam);

		m_lastMousePos = IVector2(mouseX, mouseY);

		m_isMouseButtonDown = false;
		m_mouseButtonDownFrameCount = 0;

		TB8::EventMessage* event = TB8::EventMessage_MouseLBUp::Alloc(mouseX, mouseY);
		m_eventQueue->QueueMessage(&event);
		ReleaseCapture();
		break;
	}
	case WM_LBUTTONDBLCLK:
	{
		const IVector2 mousePos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		m_lastMousePos = mousePos;

		TB8::EventMessage* event = TB8::EventMessage_MouseLBDoubleClick::Alloc(mousePos);
		m_eventQueue->QueueMessage(&event);
		SetCapture(m_hWnd);
		break;
	}
	case WM_SIZE:
	{
		IVector2 snappedSize(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		m_pRenderer->AlignScreenSize(snappedSize);

		RECT r;
		r.left = 0;
		r.right = snappedSize.x;
		r.top = 0;
		r.bottom = snappedSize.y;

		AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

		UINT uFlags = SWP_NOMOVE | SWP_NOOWNERZORDER |
			SWP_NOZORDER | SWP_SHOWWINDOW;
		SetWindowPos(m_hWnd, HWND_TOP, 0, 0, r.right - r.left, r.bottom - r.top, uFlags);

		TB8::EventMessage* event = TB8::EventMessage_ResizeWindow::Alloc(snappedSize.x, snappedSize.y, true);
		m_eventQueue->QueueMessage(&event);
		return TRUE;
		break;
	}
	case WM_DPICHANGED:
	{
		const IVector2 dpi(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
		const RECT* prcNewWindow = reinterpret_cast<RECT*>(lParam);
		const IVector2 size(prcNewWindow->right - prcNewWindow->left, prcNewWindow->bottom - prcNewWindow->top);
		TB8::EventMessage* event = TB8::EventMessage_WindowDPIChanged::Alloc(dpi, size);
		m_eventQueue->QueueMessage(&event);

		SetWindowPos(hWnd,
			NULL,
			prcNewWindow->left,
			prcNewWindow->top,
			prcNewWindow->right - prcNewWindow->left,
			prcNewWindow->bottom - prcNewWindow->top,
			SWP_NOZORDER | SWP_NOACTIVATE);
		break;
	}
	case WM_SIZING:
	{
		RECT* screenRect = reinterpret_cast<RECT*>(lParam);

		POINT UL = { screenRect->left, screenRect->top };
		ScreenToClient(hWnd, &UL);

		POINT LR = { screenRect->right, screenRect->bottom };
		ScreenToClient(hWnd, &LR);

		IVector2 clientSize(screenRect->right - screenRect->left, screenRect->bottom - screenRect->top);
		IVector2 snappedSize = clientSize;
		m_pRenderer->AlignScreenSize(snappedSize);

		screenRect->right += snappedSize.x - clientSize.x;
		screenRect->bottom += snappedSize.y -  clientSize.y;
		return TRUE;
		break;
	}
	case WM_ENTERSIZEMOVE:
		m_inSizeMove = true;
		SetTimer(m_hWnd, 0, 1000 / 60, nullptr);
		break;

	case WM_EXITSIZEMOVE:
		m_inSizeMove = false;
		KillTimer(m_hWnd, 0);
		break;

	case WM_TIMER:
		if (m_inSizeMove)
		{
			__DispatchEventQueue();
			__ProcessRenderFrame();

			SetTimer(m_hWnd, 0, 1000 / 60, nullptr);
		}
		break;

	case WM_ERASEBKGND:
		return TRUE;
		break;

	case WM_PAINT:
		break;

	case WM_SETCURSOR:
	{
		if (m_pRenderer->IsCursorVisible())
		{
			return TRUE;
		}
		break;
	}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

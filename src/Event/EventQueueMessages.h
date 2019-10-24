#pragma once

#include "common/basic_types.h"

#include "EventQueueMessage.h"

namespace TB8
{

enum RenderScaleSize : u32;

// list of message ids.
enum EventMessageID : u32
{
	EventMessageID_Invalid = 0,

	EventMessageID_MoveStart,
	EventMessageID_MoveEnd,
	EventMessageID_ResizeWindow,
	EventMessageID_WindowDPIChanged,
	EventMessageID_RenderAreaResize,

	EventMessageID_Activate,
	EventMessageID_Rotate,
	EventMessageID_Close,

	EventMessageID_MouseCursorMove,
	EventMessageID_MouseLBDown,
	EventMessageID_MouseLBDownHold,
	EventMessageID_MouseLBUp,
	EventMessageID_MouseLeave,
	EventMessageID_MouseHover,
	EventMessageID_MouseLBDoubleClick,
	EventMessageID_MouseMoveWheel,
};

// EventMessage_StartMove
class EventMessage_MoveStart : public EventMessage
{
public:
	EventMessage_MoveStart(s32 dx, s32 dy);

	static EventMessage_MoveStart* Alloc(s32 dx, s32 dy);
	virtual void __Free() override;

	s32 m_dx;
	s32 m_dy;
};

// EventMessage_EndMove
class EventMessage_MoveEnd : public EventMessage
{
public:
	EventMessage_MoveEnd(s32 dx, s32 dy);

	static EventMessage_MoveEnd* Alloc(s32 dx, s32 dy);
	virtual void __Free() override;

	s32 m_dx;
	s32 m_dy;
};

// EventMessage_ResizeWindow
class EventMessage_ResizeWindow : public EventMessage
{
public:
	EventMessage_ResizeWindow(s32 sizeX, s32 sizeY, bool final);

	static EventMessage_ResizeWindow* Alloc(s32 sizeX, s32 sizeY, bool final);
	virtual void __Free() override;

	s32 m_sizeX;
	s32 m_sizeY;
	bool m_final;
};

// EventMessageID_RenderAreaResize
class EventMessage_RenderAreaResize : public EventMessage
{
public:
	EventMessage_RenderAreaResize(s32 dpi, RenderScaleSize scale, const IVector2& size);

	static EventMessage_RenderAreaResize* Alloc(s32 dpi, RenderScaleSize scale, const IVector2& size);
	virtual void __Free() override;

	s32 m_dpi;
	RenderScaleSize m_scale;
	IVector2 m_size;
};

// EventMessageID_WindowDPIChanged
class EventMessage_WindowDPIChanged : public EventMessage
{
public:
	EventMessage_WindowDPIChanged(const IVector2& dpi, const IVector2& size);

	static EventMessage_WindowDPIChanged* Alloc(const IVector2& dpi, const IVector2& size);
	virtual void __Free() override;

	IVector2 m_dpi;
	IVector2 m_size;
};

// EventMessageID_Activate
class EventMessage_Activate : public EventMessage
{
public:
	EventMessage_Activate();

	static EventMessage_Activate* Alloc();
	virtual void __Free() override;
};

// EventMessageID_Rotate
class EventMessage_Rotate : public EventMessage
{
public:
	EventMessage_Rotate();

	static EventMessage_Rotate* Alloc();
	virtual void __Free() override;
};

// EventMessageID_Close
class EventMessage_Close : public EventMessage
{
public:
	EventMessage_Close();

	static EventMessage_Close* Alloc();
	virtual void __Free() override;
};

// EventMessage_MouseCursorMove
class EventMessage_MouseCursorMove : public EventMessage
{
public:
	EventMessage_MouseCursorMove(s32 mouseX, s32 mouseY);

	static EventMessage_MouseCursorMove* Alloc(s32 mouseX, s32 mouseY);
	virtual void __Free() override;

	s32 m_mouseX;
	s32 m_mouseY;
};

// EventMessage_MouseLBDown
class EventMessage_MouseLBDown : public EventMessage
{
public:
	EventMessage_MouseLBDown(s32 mouseX, s32 mouseY);

	static EventMessage_MouseLBDown* Alloc(s32 mouseX, s32 mouseY);
	virtual void __Free() override;

	s32 m_mouseX;
	s32 m_mouseY;
};

// EventMessageID_MouseLBDownHold
class EventMessage_MouseLBDownHold : public EventMessage
{
public:
	EventMessage_MouseLBDownHold(u32 frameCount, const IVector2& mousePos);

	static EventMessage_MouseLBDownHold* Alloc(u32 frameCount, const IVector2& mousePos);
	virtual void __Free() override;

	u32 m_frameCount;
	IVector2 m_mousePos;
};

// EventMessageID_MouseLBDoubleClick
class EventMessage_MouseLBDoubleClick : public EventMessage
{
public:
	EventMessage_MouseLBDoubleClick(const IVector2& mousePos);

	static EventMessage_MouseLBDoubleClick* Alloc(const IVector2& mousePos);
	virtual void __Free() override;

	IVector2 m_mousePos;
};

// EventMessage_MouseLBUp
class EventMessage_MouseLBUp : public EventMessage
{
public:
	EventMessage_MouseLBUp(s32 mouseX, s32 mouseY);

	static EventMessage_MouseLBUp* Alloc(s32 mouseX, s32 mouseY);
	virtual void __Free() override;

	s32 m_mouseX;
	s32 m_mouseY;
};

// EventMessageID_MouseLeave
class EventMessage_MouseLeave : public EventMessage
{
public:
	EventMessage_MouseLeave(const IVector2& mousePos);

	static EventMessage_MouseLeave* Alloc(const IVector2& mousePos);
	virtual void __Free() override;

	IVector2 m_mousePos;
};

// EventMessageID_MouseHover
class EventMessage_MouseHover : public EventMessage
{
public:
	EventMessage_MouseHover(const IVector2& mousePos);

	static EventMessage_MouseHover* Alloc(const IVector2& mousePos);
	virtual void __Free() override;

	IVector2 m_mousePos;
};

// EventMessageID_MouseMoveWheel
class EventMessage_MouseMoveWheel : public EventMessage
{
public:
	EventMessage_MouseMoveWheel(const IVector2& mousePos, s32 delta, s32 threshold);

	static EventMessage_MouseMoveWheel* Alloc(const IVector2& mousePos, s32 delta, s32 threshold);
	virtual void __Free() override;

	IVector2 m_mousePos;
	s32 m_delta;
	s32 m_threshold;
};

}

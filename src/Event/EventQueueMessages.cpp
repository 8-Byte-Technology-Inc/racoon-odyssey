/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/
#include "pch.h"

#include "EventQueueMessages.h"

namespace TB8
{

// EventMessage_MoveStart
EventMessage_MoveStart::EventMessage_MoveStart(s32 dx, s32 dy)
	: EventMessage(EventMessageID_MoveStart)
	, m_dx(dx)
	, m_dy(dy)
{
}

EventMessage_MoveStart* EventMessage_MoveStart::Alloc(s32 dx, s32 dy)
{
	EventMessage_MoveStart* obj = TB8_NEW(EventMessage_MoveStart)(dx, dy);
	return obj;
}

void EventMessage_MoveStart::__Free()
{
	TB8_DEL(this);
}

// EventMessage_EndMove
EventMessage_MoveEnd::EventMessage_MoveEnd(s32 dx, s32 dy)
	: EventMessage(EventMessageID_MoveEnd)
	, m_dx(dx)
	, m_dy(dy)
{
}

EventMessage_MoveEnd* EventMessage_MoveEnd::Alloc(s32 dx, s32 dy)
{
	EventMessage_MoveEnd* obj = TB8_NEW(EventMessage_MoveEnd)(dx, dy);
	return obj;
}

void EventMessage_MoveEnd::__Free()
{
	TB8_DEL(this);
}

// EventMessage_ResizeWindow
EventMessage_ResizeWindow::EventMessage_ResizeWindow(s32 sizeX, s32 sizeY, bool final)
	: EventMessage(EventMessageID_ResizeWindow)
	, m_sizeX(sizeX)
	, m_sizeY(sizeY)
	, m_final(final)
{
}

EventMessage_ResizeWindow* EventMessage_ResizeWindow::Alloc(s32 sizeX, s32 sizeY, bool final)
{
	EventMessage_ResizeWindow* obj = TB8_NEW(EventMessage_ResizeWindow)(sizeX, sizeY, final);
	return obj;
}

void EventMessage_ResizeWindow::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_WindowDPIChanged
EventMessage_WindowDPIChanged::EventMessage_WindowDPIChanged(const IVector2& dpi, const IVector2& size)
	: EventMessage(EventMessageID_WindowDPIChanged)
	, m_dpi(dpi)
	, m_size(size)
{
}

EventMessage_WindowDPIChanged* EventMessage_WindowDPIChanged::Alloc(const IVector2& dpi, const IVector2& size)
{
	EventMessage_WindowDPIChanged* obj = TB8_NEW(EventMessage_WindowDPIChanged)(dpi, size);
	return obj;
}

void EventMessage_WindowDPIChanged::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_RenderAreaResize
EventMessage_RenderAreaResize::EventMessage_RenderAreaResize(s32 dpi, RenderScaleSize scale, const IVector2& size)
	: EventMessage(EventMessageID_RenderAreaResize)
	, m_dpi(dpi)
	, m_scale(scale)
	, m_size(size)
{
}

EventMessage_RenderAreaResize* EventMessage_RenderAreaResize::Alloc(s32 dpi, RenderScaleSize scale, const IVector2& size)
{
	EventMessage_RenderAreaResize* obj = TB8_NEW(EventMessage_RenderAreaResize)(dpi, scale, size);
	return obj;
}

void EventMessage_RenderAreaResize::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_Activate
EventMessage_Activate::EventMessage_Activate()
	: EventMessage(EventMessageID_Activate)
{
}

EventMessage_Activate* EventMessage_Activate::Alloc()
{
	EventMessage_Activate* obj = TB8_NEW(EventMessage_Activate)();
	return obj;
}

void EventMessage_Activate::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_Rotate
EventMessage_Rotate::EventMessage_Rotate()
	: EventMessage(EventMessageID_Rotate)
{
}

EventMessage_Rotate* EventMessage_Rotate::Alloc()
{
	EventMessage_Rotate* obj = TB8_NEW(EventMessage_Rotate)();
	return obj;
}

void EventMessage_Rotate::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_Close
EventMessage_Close::EventMessage_Close()
	: EventMessage(EventMessageID_Close)
{
}

EventMessage_Close* EventMessage_Close::Alloc()
{
	EventMessage_Close* obj = TB8_NEW(EventMessage_Close)();
	return obj;
}

void EventMessage_Close::__Free()
{
	TB8_DEL(this);
}

// EventMessage_MouseCursorMove
EventMessage_MouseCursorMove::EventMessage_MouseCursorMove(s32 mouseX, s32 mouseY)
	: EventMessage(EventMessageID_MouseCursorMove)
	, m_mouseX(mouseX)
	, m_mouseY(mouseY)
{
}

EventMessage_MouseCursorMove* EventMessage_MouseCursorMove::Alloc(s32 mouseX, s32 mouseY)
{
	EventMessage_MouseCursorMove* obj = TB8_NEW(EventMessage_MouseCursorMove)(mouseX, mouseY);
	return obj;
}

void EventMessage_MouseCursorMove::__Free()
{
	TB8_DEL(this);
}

// EventMessage_MouseLBDown
EventMessage_MouseLBDown::EventMessage_MouseLBDown(s32 mouseX, s32 mouseY)
	: EventMessage(EventMessageID_MouseLBDown)
	, m_mouseX(mouseX)
	, m_mouseY(mouseY)
{
}

EventMessage_MouseLBDown* EventMessage_MouseLBDown::Alloc(s32 mouseX, s32 mouseY)
{
	EventMessage_MouseLBDown* obj = TB8_NEW(EventMessage_MouseLBDown)(mouseX, mouseY);
	return obj;
}

void EventMessage_MouseLBDown::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_MouseLBDownHold
EventMessage_MouseLBDownHold::EventMessage_MouseLBDownHold(u32 frameCount, const IVector2& mousePos)
	: EventMessage(EventMessageID_MouseLBDownHold)
	, m_frameCount(frameCount)
	, m_mousePos(mousePos)
{
}

EventMessage_MouseLBDownHold* EventMessage_MouseLBDownHold::Alloc(u32 frameCount, const IVector2& mousePos)
{
	EventMessage_MouseLBDownHold* obj = TB8_NEW(EventMessage_MouseLBDownHold)(frameCount, mousePos);
	return obj;
}

void EventMessage_MouseLBDownHold::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_MouseLBDoubleClick
EventMessage_MouseLBDoubleClick::EventMessage_MouseLBDoubleClick(const IVector2& mousePos)
	: EventMessage(EventMessageID_MouseLBDoubleClick)
	, m_mousePos(mousePos)
{
}

EventMessage_MouseLBDoubleClick* EventMessage_MouseLBDoubleClick::Alloc(const IVector2& mousePos)
{
	EventMessage_MouseLBDoubleClick* obj = TB8_NEW(EventMessage_MouseLBDoubleClick)(mousePos);
	return obj;
}

void EventMessage_MouseLBDoubleClick::__Free()
{
	TB8_DEL(this);
}

// EventMessage_MouseLBUp
EventMessage_MouseLBUp::EventMessage_MouseLBUp(s32 mouseX, s32 mouseY)
	: EventMessage(EventMessageID_MouseLBUp)
	, m_mouseX(mouseX)
	, m_mouseY(mouseY)
{
}

EventMessage_MouseLBUp* EventMessage_MouseLBUp::Alloc(s32 mouseX, s32 mouseY)
{
	EventMessage_MouseLBUp* obj = TB8_NEW(EventMessage_MouseLBUp)(mouseX, mouseY);
	return obj;
}

void EventMessage_MouseLBUp::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_MouseLeave
EventMessage_MouseLeave::EventMessage_MouseLeave(const IVector2& mousePos)
	: EventMessage(EventMessageID_MouseLeave)
	, m_mousePos(mousePos)
{
}

EventMessage_MouseLeave* EventMessage_MouseLeave::Alloc(const IVector2& mousePos)
{
	EventMessage_MouseLeave* obj = TB8_NEW(EventMessage_MouseLeave)(mousePos);
	return obj;
}

void EventMessage_MouseLeave::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_MouseHover
EventMessage_MouseHover::EventMessage_MouseHover(const IVector2& mousePos)
	: EventMessage(EventMessageID_MouseHover)
	, m_mousePos(mousePos)
{
}

EventMessage_MouseHover* EventMessage_MouseHover::Alloc(const IVector2& mousePos)
{
	EventMessage_MouseHover* obj = TB8_NEW(EventMessage_MouseHover)(mousePos);
	return obj;
}

void EventMessage_MouseHover::__Free()
{
	TB8_DEL(this);
}

// EventMessageID_MouseMoveWheel
EventMessage_MouseMoveWheel::EventMessage_MouseMoveWheel(const IVector2& mousePos, s32 delta, s32 threshold)
	: EventMessage(EventMessageID_MouseMoveWheel)
	, m_mousePos(mousePos)
	, m_delta(delta)
	, m_threshold(threshold)
{
}

EventMessage_MouseMoveWheel* EventMessage_MouseMoveWheel::Alloc(const IVector2& mousePos, s32 delta, s32 threshold)
{
	EventMessage_MouseMoveWheel* obj = TB8_NEW(EventMessage_MouseMoveWheel)(mousePos, delta, threshold);
	return obj;
}

void EventMessage_MouseMoveWheel::__Free()
{
	TB8_DEL(this);
}

}

#include "pch.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "common/memory.h"

#include "RenderHelper.h"
#include "RenderTexture.h"
#include "RenderMain.h"

#include "RenderImagine.h"

namespace TB8
{

RenderImagine* RenderImagine::Alloc(RenderMain* pRenderer)
{
	RenderImagine* pObj = TB8_NEW(RenderImagine)(pRenderer);
	pObj->__Initialize();
	return pObj;
}

void RenderImagine::__Free()
{
	TB8_DEL(this);
}

RenderImagine::RenderImagine(RenderMain* pRenderer)
	: m_pRenderer(pRenderer)
{
}

RenderImagine::~RenderImagine()
{
}

void RenderImagine::__Initialize()
{
}

void RenderImagine::SetSize(const IVector2& size)
{
	m_size = size;
}

void RenderImagine::SetPosition(const IVector2& pos)
{
	m_pos = pos;
}

void RenderImagine::Render()
{
	ID2D1PathGeometry* pPathGeometry = nullptr;
	ID2D1GeometrySink* pSink = nullptr;
	HRESULT hr = S_OK;

	hr = m_pRenderer->GetD2DFactory()->CreatePathGeometry(&pPathGeometry);
	assert(SUCCEEDED(hr));

	hr = pPathGeometry->Open(&pSink);
	assert(SUCCEEDED(hr));

	Vector2 ul(static_cast<f32>(m_pos.x - m_size.x / 2), static_cast<f32>(m_pos.y - m_size.y / 2));
	Vector2 ur(static_cast<f32>(m_pos.x + m_size.x / 2), static_cast<f32>(m_pos.y - m_size.y / 2));
	Vector2 ll(static_cast<f32>(m_pos.x - m_size.x / 2), static_cast<f32>(m_pos.y + m_size.y / 2));
	Vector2 lr(static_cast<f32>(m_pos.x + m_size.x / 2), static_cast<f32>(m_pos.y + m_size.y / 2));

	pSink->BeginFigure(D2D1::Point2F(ul.x, ul.y), D2D1_FIGURE_BEGIN_FILLED);

	// top.
	{
		const s32 segX = m_size.x / 40;
		const f32 offsetX = static_cast<f32>(m_size.x) / (static_cast<f32>(segX) * 2.f);
		const f32 offsetY = 20.f;

		Vector2 p0 = ul;

		for (s32 i = 0; i < segX; ++i)
		{
			Vector2 p2(p0.x + offsetX, p0.y - offsetY);
			Vector2 p4(p2.x + offsetX, p2.y + offsetY);

			Vector2 p1(p0.x, p2.y);
			Vector2 p3(p4.x, p2.y);

			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p0.x, p0.y), D2D1::Point2F(p1.x, p1.y), D2D1::Point2F(p2.x, p2.y)));
			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p2.x, p2.y), D2D1::Point2F(p3.x, p3.y), D2D1::Point2F(p4.x, p4.y)));

			p0 = p4;
		}
	}

	// right.
	{
		const s32 segY = m_size.y / 40;
		const f32 offsetX = 20.f;
		const f32 offsetY = static_cast<f32>(m_size.y) / (static_cast<f32>(segY) * 2.f);

		Vector2 p0 = ur;

		for (s32 i = 0; i < segY; ++i)
		{
			Vector2 p2(p0.x + offsetX, p0.y + offsetY);
			Vector2 p4(p2.x - offsetX, p2.y + offsetY);

			Vector2 p1(p2.x, p0.y);
			Vector2 p3(p2.x, p4.y);

			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p0.x, p0.y), D2D1::Point2F(p1.x, p1.y), D2D1::Point2F(p2.x, p2.y)));
			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p2.x, p2.y), D2D1::Point2F(p3.x, p3.y), D2D1::Point2F(p4.x, p4.y)));

			p0 = p4;
		}
	}

	// bottom.
	{
		const s32 segX = m_size.x / 40;
		const f32 offsetX = static_cast<f32>(m_size.x) / (static_cast<f32>(segX) * 2.f);
		const f32 offsetY = 20.f;

		Vector2 p0 = lr;

		for (s32 i = 0; i < segX; ++i)
		{
			Vector2 p2(p0.x - offsetX, p0.y + offsetY);
			Vector2 p4(p2.x - offsetX, p2.y - offsetY);

			Vector2 p1(p0.x, p2.y);
			Vector2 p3(p4.x, p2.y);

			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p0.x, p0.y), D2D1::Point2F(p1.x, p1.y), D2D1::Point2F(p2.x, p2.y)));
			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p2.x, p2.y), D2D1::Point2F(p3.x, p3.y), D2D1::Point2F(p4.x, p4.y)));

			p0 = p4;
		}
	}

	// left.
	{
		const s32 segY = m_size.y / 40;
		const f32 offsetX = 20.f;
		const f32 offsetY = static_cast<f32>(m_size.y) / (static_cast<f32>(segY) * 2.f);

		Vector2 p0 = ll;

		for (s32 i = 0; i < segY; ++i)
		{
			Vector2 p2(p0.x - offsetX, p0.y - offsetY);
			Vector2 p4(p2.x + offsetX, p2.y - offsetY);

			Vector2 p1(p2.x, p0.y);
			Vector2 p3(p2.x, p4.y);

			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p0.x, p0.y), D2D1::Point2F(p1.x, p1.y), D2D1::Point2F(p2.x, p2.y)));
			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p2.x, p2.y), D2D1::Point2F(p3.x, p3.y), D2D1::Point2F(p4.x, p4.y)));

			p0 = p4;
		}
	}

	pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
	pSink->Close();
	RELEASEI(pSink);

	ID2D1SolidColorBrush* pBlackBrush = nullptr;
	hr = m_pRenderer->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBlackBrush);
	assert(SUCCEEDED(hr));

	ID2D1SolidColorBrush* pWhiteBrush = nullptr;
	hr = m_pRenderer->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pWhiteBrush);
	assert(SUCCEEDED(hr));

	m_pRenderer->GetD2DDeviceContext()->DrawGeometry(pPathGeometry, pBlackBrush, 4.f);
	m_pRenderer->GetD2DDeviceContext()->FillGeometry(pPathGeometry, pWhiteBrush);

	RELEASEI(pBlackBrush);
	RELEASEI(pWhiteBrush);
	RELEASEI(pPathGeometry);
}

}

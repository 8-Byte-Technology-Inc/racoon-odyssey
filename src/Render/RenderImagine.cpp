#include "pch.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "common/memory.h"

#include "RenderHelper.h"
#include "RenderScale.h"
#include "RenderTexture.h"
#include "RenderMain.h"

#include "RenderImagine.h"

namespace TB8
{

const u32 BASE_SPACING_PIXELS = 4;
const u32 CIRCLE_COUNT = 3;

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
	, m_pszText(nullptr)
	, m_pBubbleGeometry(nullptr)
{
}

RenderImagine::~RenderImagine()
{
	TB8_FREE(m_pszText);
	RELEASEI(m_pBubbleGeometry);
	__CleanupLayouts();
}

void RenderImagine::__Initialize()
{
}

void RenderImagine::SetPosition(const Vector2& pos)
{
	if (m_pos == pos)
		return;
	m_pos = pos;
	__ComputeBubbleGeometry();
}

void RenderImagine::SetText(const char* pszText)
{
	// convert the text to unicode.
	TB8_FREE(m_pszText);
	const int lenUnicode = MultiByteToWideChar(CP_UTF8, 0, pszText, -1, nullptr, 0);
	m_pszText = static_cast<WCHAR*>(TB8_MALLOC(lenUnicode * sizeof(WCHAR)));
	MultiByteToWideChar(CP_UTF8, 0, pszText, -1, m_pszText, lenUnicode);

	// compute layout.
	__ComputeLayout_Text();
	__ComputeBubbleGeometry();
}

void RenderImagine::__CleanupLayouts()
{
	// cleanup old layouts.
	for (std::vector<RenderImagine_TextLine>::iterator it = m_layouts.begin(); it != m_layouts.end(); ++it)
	{
		RenderImagine_TextLine& layout = *it;
		RELEASEI(layout.m_pLayout);
	}
}

struct RenderImagine_Word
{
	WCHAR* m_pszStart;
	WCHAR* m_pszEnd;
	f32 m_width;
};

void RenderImagine::__ComputeLayout_Text()
{
	HRESULT hr = S_OK;

	const f32 spacing = static_cast<f32>(ComputeViewPixelsFromHardPixels(BASE_SPACING_PIXELS, m_pRenderer->GetRenderScaleSize()));
	const Vector2 spacingSize(spacing, spacing);

	IDWriteTextFormat* pTextFormat = m_pRenderer->GetFont(RenderFontID_Imagine);

	// determine the widths of each word.
	std::vector<RenderImagine_Word> words;
	f32 maxHeight = 0.f;
	f32 totalWidth = 0.f;
	WCHAR* psz = m_pszText;
	while (*psz != 0)
	{
		while ((*psz != 0) && (*psz <= 0x20))
			++psz;
		if (*psz == 0)
			break;
		WCHAR* pszStart = psz;
		while (*psz > 0x20)
			++psz;

		IDWriteTextLayout* pTextLayout;
		const UINT32 stringLength = static_cast<UINT32>(psz - pszStart);
		hr = m_pRenderer->GetWriteFactory()->CreateTextLayout(
			pszStart,			// [in]  const WCHAR*			string,
			stringLength,		// UINT32						stringLength,
			pTextFormat,		// IDWriteTextFormat * textFormat,
			1024.f,				// FLOAT              maxWidth,
			1024.f,				// FLOAT              maxHeight,
			&pTextLayout);		// [out]       IDWriteTextLayout ** textLayout
		assert(hr == S_OK);

		// determine the size.
		DWRITE_TEXT_METRICS textMetrics;
		hr = pTextLayout->GetMetrics(&textMetrics);
		assert(hr == S_OK);
		RELEASEI(pTextLayout);

		maxHeight = std::max<f32>(maxHeight, textMetrics.height);

		words.emplace_back();
		RenderImagine_Word& word = words.back();
		word.m_pszStart = pszStart;
		word.m_pszEnd = psz;
		word.m_width = textMetrics.width;
		totalWidth += textMetrics.width;
	}

	const f32 textLineHeight = static_cast<f32>(static_cast<u32>(maxHeight + 0.999f));

	// determine desired width of each line.
	std::vector<f32> lineWidthsDesired;
	if (words.size() > 1)
	{
		const u32 lineCount = static_cast<u32>(sqrt(totalWidth / (2.f * maxHeight)));
		const u32 lineMid = (lineCount & 1) ? (lineCount / 2) : (lineCount / 2) - 1;
		const f32 lineWidth = totalWidth / static_cast<f32>(lineCount);
		const f32 lineMult = 0.15f;
		const f32 lineDelta = lineWidth * lineMult;

		const s32 f0 = (lineCount & 1) ? (lineCount / 2) : ((lineCount - 1) / 2);
		const s32 f1 = f0 * f0 + f0;
		const f32 f2 = lineMult * (static_cast<f32>(f1) / static_cast<f32>(lineCount)) + 1.f;

		const f32 lineWidthMax = lineWidth * f2;

		lineWidthsDesired.resize(lineCount);

		f32 lineWidthI = lineWidthMax;
		for (s32 i = lineMid; i >= 0; --i, lineWidthI -= lineDelta)
		{
			lineWidthsDesired[i] = lineWidthI;
			lineWidthsDesired[lineCount - i - 1] = lineWidthI;
		}
	}
	else
	{
		lineWidthsDesired.resize(1);
		lineWidthsDesired[0] = totalWidth;
	}

	// populate text layouts.
	__CleanupLayouts();
	m_layouts.resize(lineWidthsDesired.size());
	{
		u32 iLine = 0;
		f32 lineWidth = 0.f;
		WCHAR* pszLineStart = nullptr;
		WCHAR* pszLineEnd = nullptr;

		for (std::vector<RenderImagine_Word>::const_iterator itWord = words.begin(); itWord != words.end(); ++itWord)
		{
			const RenderImagine_Word& word = *itWord;

			// is this line full?
			if ((lineWidth > 0.f) && ((iLine + 1) < lineWidthsDesired.size()) && ((lineWidth + (word.m_width / 2.f)) > lineWidthsDesired[iLine]))
			{
				// create a layout for this line.
				RenderImagine_TextLine& layout = m_layouts[iLine];
				layout.m_index = iLine;
				const UINT32 stringLen = static_cast<UINT32>(pszLineEnd - pszLineStart);
				hr = m_pRenderer->GetWriteFactory()->CreateTextLayout(
					pszLineStart,								// [in]  const WCHAR*			string,
					stringLen,									// UINT32						stringLength,
					pTextFormat,								// IDWriteTextFormat * textFormat,
					4096.f,										// FLOAT              maxWidth,
					128.f,										// FLOAT              maxHeight,
					&(layout.m_pLayout));						// [out]       IDWriteTextLayout ** textLayout
				assert(hr == S_OK);

				++iLine;
				pszLineStart = nullptr;
				lineWidth = 0.f;
			}

			// add this word to the line.
			if (!pszLineStart)
			{
				pszLineStart = word.m_pszStart;
			}
			pszLineEnd = word.m_pszEnd;
			lineWidth += word.m_width;
		}

		if (lineWidth > 0.f)
		{
			RenderImagine_TextLine& layout = m_layouts.back();
			layout.m_index = iLine;
			const UINT32 stringLen = static_cast<UINT32>(pszLineEnd - pszLineStart);
			hr = m_pRenderer->GetWriteFactory()->CreateTextLayout(
				pszLineStart,								// [in]  const WCHAR*			string,
				stringLen,									// UINT32						stringLength,
				pTextFormat,								// IDWriteTextFormat * textFormat,
				4096.f,										// FLOAT              maxWidth,
				128.f,										// FLOAT              maxHeight,
				&(layout.m_pLayout));						// [out]       IDWriteTextLayout ** textLayout
			assert(hr == S_OK);
		}
	}

	// compute line sizes.
	f32 widthTextTotal = 0.f;
	for (std::vector<RenderImagine_TextLine>::iterator it = m_layouts.begin(); it != m_layouts.end(); ++it)
	{
		RenderImagine_TextLine& layout = *it;

		// determine the size.
		DWRITE_TEXT_METRICS textMetrics;
		hr = layout.m_pLayout->GetMetrics(&textMetrics);
		layout.m_size.x = textMetrics.width;
		layout.m_size.y = textLineHeight;
		assert(hr == S_OK);

		widthTextTotal = std::max<f32>(widthTextTotal, layout.m_size.x);
	}

	const f32 heightTextTotal = (textLineHeight * static_cast<f32>(m_layouts.size())) + (static_cast<f32>(m_layouts.size() - 1) * spacingSize.y);
	const Vector2 textSize(widthTextTotal, heightTextTotal);

	// compute position for circles.
	Vector2 offsetPosToCircle(0.f, 0.f);
	f32 radius = textLineHeight / 8.f;
	for (u32 i = 0; i < CIRCLE_COUNT; ++i)
	{
		offsetPosToCircle.x -= radius * 2.f;
		offsetPosToCircle.y -= radius * 2.f;

		radius += textLineHeight / 8.f;
	}

	// compute UL position for each line.
	const Vector2 offsetPosToLR = offsetPosToCircle - spacingSize;
	const Vector2 offsetPosToUL = offsetPosToLR - textSize;

	for (std::vector<RenderImagine_TextLine>::iterator it = m_layouts.begin(); it != m_layouts.end(); ++it)
	{
		RenderImagine_TextLine& layout = *it;
		layout.m_offsetPosToUL.x = offsetPosToUL.x + (textSize.x / 2.f) - (layout.m_size.x / 2.f);
		layout.m_offsetPosToUL.y = offsetPosToUL.y + (layout.m_index) * (textLineHeight + spacingSize.y);
	}

	// compute text size.
	m_textSize = offsetPosToLR - offsetPosToUL;

	// compute center pos.
	m_posToCenter = (offsetPosToUL + offsetPosToLR) / 2.f;

	__AddBubblePoints();
	__AdjustBubblePointSegments();
	__AdjustBubblePointConvex();
	for (u32 i = 0; i < 3; ++i)
	{
		__AdjustBubblePointSpacing();
	}
	__AddCircles();
}

void RenderImagine::__AddBubblePoints()
{
	const f32 spacing = static_cast<f32>(ComputeViewPixelsFromHardPixels(BASE_SPACING_PIXELS, m_pRenderer->GetRenderScaleSize()));
	const Vector2 spacingSize(spacing, spacing);

	// top.
	{
		RenderImagine_TextLine& line = m_layouts.front();
		const Vector2 p0(line.m_offsetPosToUL.x - spacingSize.x, line.m_offsetPosToUL.y - spacingSize.y);
		m_bubblePoints.push_back(p0);
		const Vector2 p1(line.m_offsetPosToUL.x + line.m_size.x + spacingSize.x, line.m_offsetPosToUL.y - spacingSize.y);
		m_bubblePoints.push_back(p1);
	}

	// right.
	{
		for (std::vector<RenderImagine_TextLine>::iterator it = m_layouts.begin(); it != m_layouts.end(); ++it)
		{
			RenderImagine_TextLine& line = *it;
			const Vector2 p0(line.m_offsetPosToUL.x + line.m_size.x + spacingSize.x, line.m_offsetPosToUL.y - spacingSize.y);

			if (m_bubblePoints.back().x < p0.x)
			{
				m_bubblePoints.back() = p0;
			}

			const Vector2 p1(line.m_offsetPosToUL.x + line.m_size.x + spacingSize.x, line.m_offsetPosToUL.y + line.m_size.y + spacingSize.y);
			m_bubblePoints.push_back(p1);
		}
	}

	// bottom.
	{
		RenderImagine_TextLine& line = m_layouts.back();
		const Vector2 p0(line.m_offsetPosToUL.x + line.m_size.x + spacingSize.x, line.m_offsetPosToUL.y + line.m_size.y + spacingSize.y);
		m_bubblePoints.push_back(p0);
		const Vector2 p1(line.m_offsetPosToUL.x - spacingSize.x, line.m_offsetPosToUL.y + line.m_size.y + spacingSize.y);
		m_bubblePoints.push_back(p1);
	}

	// left.
	{
		for (std::vector<RenderImagine_TextLine>::reverse_iterator it = m_layouts.rbegin(); it != m_layouts.rend(); ++it)
		{
			RenderImagine_TextLine& line = *it;
			const Vector2 p0(line.m_offsetPosToUL.x - spacingSize.x, line.m_offsetPosToUL.y + line.m_size.y + spacingSize.y);

			if (m_bubblePoints.back().x > p0.x)
			{
				m_bubblePoints.back() = p0;
			}

			const Vector2 p1(line.m_offsetPosToUL.x - spacingSize.x, line.m_offsetPosToUL.y - spacingSize.y);
			m_bubblePoints.push_back(p1);
		}

	}
}

void RenderImagine::__AdjustBubblePointSegments()
{
	const f32 bubbleSize = m_layouts.front().m_size.y;

	// add / remove points to ensure spacing.
	{
		const f32 magSqMin = bubbleSize * bubbleSize;
		const f32 magSqMax = magSqMin * 4.f;

		std::vector<Vector2>::iterator it = m_bubblePoints.begin();
		while ((it != m_bubblePoints.end()) && ((it + 1) != m_bubblePoints.end()))
		{
			const Vector2& p0 = *(it);
			const Vector2& p1 = *(it + 1);
			const Vector2 v0 = p1 - p0;

			// ensure there is a reasonable distance between points.
			const f32 magSqV0 = v0.MagSq();
			if (magSqV0 < magSqMin)
			{
				// too close to next segment, delete the nearest.
				const Vector2 pC0 = p0 - m_posToCenter;
				const Vector2 pC1 = p1 - m_posToCenter;
				const f32 distC0 = pC0.Mag();
				const f32 distC1 = pC1.Mag();
				if (distC0 < distC1)
				{
					it = m_bubblePoints.erase(it);
				}
				else
				{
					it = m_bubblePoints.erase(it + 1);
					--it;
				}
			}
			else if (magSqV0 > magSqMax)
			{
				// too far from next segment, split it.
				const Vector2 p01 = ((p0 + p1) / 2.f);
				Vector2 pC01 = p01 - m_posToCenter;
				pC01.Normalize();
				const Vector2 pI = p01 + (pC01 * (bubbleSize * .1f));
				it = m_bubblePoints.insert(it + 1, pI);

				const f32 magSqVI = (pI - p0).MagSq();
				if (magSqVI > magSqMax)
					--it;
			}
			else
			{
				++it;
			}
		}
	}
}

void RenderImagine::__AdjustBubblePointSpacing()
{
	// even out distances.
	for (std::vector<Vector2>::iterator it = m_bubblePoints.begin();
		(it != m_bubblePoints.end()) && ((it + 1) != m_bubblePoints.end()) && ((it + 2) != m_bubblePoints.end());
		++it)
	{
		const Vector2& p0 = *(it);
		const Vector2& p1 = *(it + 1);
		const Vector2& p2 = *(it + 2);

		const Vector2 v0 = p1 - p0;
		const Vector2 v1 = p1 - p2;

		const f32 len0 = v0.Mag();
		const f32 len1 = v1.Mag();

		const f32 lenA = (len0 + len1) / 2.f;

		Vector2 v0u = v0;
		v0u.Normalize();

		Vector2 v1u = v1;
		v1u.Normalize();

		const Vector2 v0b = p0 + (v0u * lenA);
		const Vector2 v1b = p2 + (v1u * lenA);

		*(it + 1) = (v0b + v1b) / 2.f;
	}
}

void RenderImagine::__AdjustBubblePointConvex()
{
	const f32 bubbleSize = m_layouts.front().m_size.y;

	// ensure it's convex.
	bool foundConcave = true;
	while (foundConcave)
	{
		foundConcave = false;
		for (std::vector<Vector2>::iterator it = m_bubblePoints.begin();
			(it != m_bubblePoints.end()) && ((it + 1) != m_bubblePoints.end()) && ((it + 2) != m_bubblePoints.end()) && !foundConcave; 
			++it)
		{
			const Vector2& p0 = *(it);
			const Vector2& p1 = *(it + 1);
			const Vector2& p2 = *(it + 2);

			const Vector2 v0 = p1 - p0;
			const Vector2 v1 = p2 - p1;

			Vector3 v0n = Vector3::Normalize(Vector3(v0.x, v0.y, 0.f));
			Vector3 v1n = Vector3::Normalize(Vector3(v1.x, v1.y, 0.f));
			Vector3 vX = Vector3::Cross(v0n, v1n);

			if (is_approx_zero(vX.z) || (vX.z > 0.f))
				continue;

			foundConcave = true;
			Vector2 pC1 = p1 - m_posToCenter;
			pC1.Normalize();
			*(it + 1) = p1 + (pC1 * (bubbleSize * .1f));
		}
	}
}

void RenderImagine::__AddCircles()
{
	// compute a normal to the center.
	const Vector2 posTarget(m_posToCenter.x + m_textSize.x / 4, m_posToCenter.y);
	Vector2 n = posTarget;
	n.Normalize();

	// make a list of points the circles could potentially collide with.
	std::vector<Vector2> collisionPoints;
	collisionPoints.reserve(m_bubblePoints.size() / 2);
	for (std::vector<Vector2>::iterator it = m_bubblePoints.begin(); it != m_bubblePoints.end() && ((it + 1) != m_bubblePoints.end()); ++it)
	{
		const Vector2& p0 = *(it);
		const Vector2& p4 = *(it + 1);
		const Vector2 pN = Vector2::NormalL(p0, p4) / 2.f;
		const Vector2& p2 = ((p0 + p4) / 2.f) + pN;

		Vector2 pNn = pN;
		pNn.Normalize();
		const f32 d2 = Vector2::Dot(Vector2(0.f, 0.f) - p2, pNn);
		if (d2 < 0.f)
			continue;

		collisionPoints.push_back(p2);
	}

	// add circles that fit in the bubble.
	const f32 circleIncrement = m_layouts.front().m_size.y / 8.f;
	f32 radius = circleIncrement;
	f32 dist = radius * 2.f;
	m_circles.reserve(CIRCLE_COUNT * 2);
	Vector2 pC = n * radius;
	f32 collideDist = posTarget.Mag();
	while (true)
	{
		// does the circle collide with any bubble points?
		for (std::vector<Vector2>::iterator it = collisionPoints.begin(); (it != collisionPoints.end()) && ((it + 1) != collisionPoints.end()); ++it)
		{
			const Vector2& p0 = *(it);
			const Vector2& p1 = *(it + 1);
			const Vector2 v0C = pC - p0;
			const Vector2 v1C = pC - p1;

			Vector2 vA = p1 - p0;
			vA.Normalize();

			Vector2 vB = Vector2::NormalL(p0, p1);
			vB.Normalize();

			const f32 d0 = Vector2::Dot(v0C, vA); // distance from p0 -> pC along vA.
			const f32 d1 = Vector2::Dot(v1C, vA); // distance from p1 -> pC along vA.

			// is the circle between the two points?
			if ((d0 < -radius) || (d1 > radius))
				continue;

			const f32 dC = Vector2::Dot(v0C, vB); // distance from p0 -> pC along vB.
			collideDist = std::min<f32>(collideDist, dC);
		}

		if (collideDist < radius)
			break;

		m_circles.emplace_back();
		RenderImagine_Circle& circle = m_circles.back();

		circle.m_offsetPosToCenter = pC;
		circle.m_radius = radius;

		pC += n * (radius * 2.5f + circleIncrement);
		radius += circleIncrement;
		dist += (radius * 2.f);
	}

	// spread out the circles
	const f32 spread = (radius + collideDist) / static_cast<f32>(m_circles.size());
	f32 extra = spread;
	for (std::vector<RenderImagine_Circle>::iterator it = m_circles.begin() + 1; it != m_circles.end(); ++it)
	{
		RenderImagine_Circle& circle = *it;
		circle.m_offsetPosToCenter += (n * extra);
		extra += spread;
	}
}

void RenderImagine::__ComputeBubbleGeometry()
{
	HRESULT hr = S_OK;

	// prepare the bubble geometry.
	RELEASEI(m_pBubbleGeometry);
	hr = m_pRenderer->GetD2DFactory()->CreatePathGeometry(&m_pBubbleGeometry);
	assert(SUCCEEDED(hr));

	ID2D1GeometrySink* pSink = nullptr;
	hr = m_pBubbleGeometry->Open(&pSink);
	assert(SUCCEEDED(hr));

	Vector2 p0 = m_bubblePoints.front() + m_pos;
	pSink->BeginFigure(D2D1::Point2F(p0.x, p0.y), D2D1_FIGURE_BEGIN_FILLED);

	for (std::vector<Vector2>::const_iterator it = m_bubblePoints.begin() + 1; it != m_bubblePoints.end(); ++it)
	{
		// get two points.
		const Vector2 p4 = *it + m_pos;

		const Vector2 pN = Vector2::NormalL(p0, p4) / 2.f;

		const Vector2 p1 = p0 + pN;
		const Vector2 p2 = ((p0 + p4) / 2.f) + pN;
		const Vector2 p3 = p4 + pN;

		pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p0.x, p0.y), D2D1::Point2F(p1.x, p1.y), D2D1::Point2F(p2.x, p2.y)));
		pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(p2.x, p2.y), D2D1::Point2F(p3.x, p3.y), D2D1::Point2F(p4.x, p4.y)));

		p0 = p4;
	}

	pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
	pSink->Close();
	RELEASEI(pSink);
}

void RenderImagine::Render()
{
	HRESULT hr = S_OK;

	ID2D1SolidColorBrush* pBlackBrush = nullptr;
	hr = m_pRenderer->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBlackBrush);
	assert(SUCCEEDED(hr));

	ID2D1SolidColorBrush* pWhiteBrush = nullptr;
	hr = m_pRenderer->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pWhiteBrush);
	assert(SUCCEEDED(hr));

	// draw the little circles.
	for (std::vector<RenderImagine_Circle>::const_iterator it = m_circles.begin(); it != m_circles.end(); ++it)
	{
		const RenderImagine_Circle& circle = *it;

		const Vector2 p0 = m_pos + circle.m_offsetPosToCenter;
		const D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(p0.x, p0.y), circle.m_radius, circle.m_radius);
		m_pRenderer->GetD2DDeviceContext()->DrawEllipse(ellipse, pBlackBrush, 4.f);
		m_pRenderer->GetD2DDeviceContext()->FillEllipse(ellipse, pWhiteBrush);
	}

	// draw the bubble geometry.
	m_pRenderer->GetD2DDeviceContext()->DrawGeometry(m_pBubbleGeometry, pBlackBrush, 4.f);
	m_pRenderer->GetD2DDeviceContext()->FillGeometry(m_pBubbleGeometry, pWhiteBrush);

	// draw the text.
	for (u32 i = 0; i < m_layouts.size(); ++i)
	{
		RenderImagine_TextLine& line = m_layouts[i];
		const Vector2 p0 = m_pos + line.m_offsetPosToUL;

		m_pRenderer->GetD2DDeviceContext()->DrawTextLayout(
			D2D1::Point2F(p0.x, p0.y),		// D2D1_POINT_2F          origin,
			line.m_pLayout,					// [in] IDWriteTextLayout      *textLayout,
			pBlackBrush,					// [in] ID2D1Brush             *defaultForegroundBrush,
			D2D1_DRAW_TEXT_OPTIONS_NONE);	// D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE
	}

	RELEASEI(pBlackBrush);
	RELEASEI(pWhiteBrush);
}

}

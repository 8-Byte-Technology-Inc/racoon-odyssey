#include "pch.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "common/memory.h"

#include "RenderHelper.h"
#include "RenderScale.h"
#include "RenderTexture.h"
#include "RenderModel.h"
#include "RenderMain.h"

#include "RenderStatusBars.h"

namespace TB8
{

const u32 BASE_SPACING_PIXELS = 4;
const u32 BASE_BAR_PIXELS = 24;

RenderStatusBars* RenderStatusBars::Alloc(RenderMain* pRenderer)
{
	RenderStatusBars* pObj = TB8_NEW(RenderStatusBars)(pRenderer);
	pObj->__Initialize();
	return pObj;
}

void RenderStatusBars::__Free()
{
	TB8_DEL(this);
}

RenderStatusBars::RenderStatusBars(RenderMain* pRenderer)
	: m_pRenderer(pRenderer)
{
}

RenderStatusBars::~RenderStatusBars()
{
	__Uninitialize();
}

void RenderStatusBars::__Initialize()
{
}

void RenderStatusBars::__Uninitialize()
{
	for (std::vector<RenderStatusBars_Bar>::iterator it = m_bars.begin(); it != m_bars.end(); ++it)
	{
		RenderStatusBars_Bar& bar = *it;
		RELEASEI(bar.m_pLayout);
		RELEASEI(bar.m_pTexture);
		RELEASEI(bar.m_pModel_Whole);
		RELEASEI(bar.m_pModel_Partial);
	}
	m_bars.clear();
}

u32 RenderStatusBars::AddBar(const char* pszName, RenderTexture* pTexture, const Vector2& uv0, const Vector2& uv1, s32 initialValue, s32 maxValue, s32 incrementSize)
{
	HRESULT hr = S_OK;
	m_bars.emplace_back();
	RenderStatusBars_Bar& bar = m_bars.back();
	bar.m_barID = static_cast<u32>(m_bars.size());
	bar.m_pLayout = nullptr;
	bar.m_value = initialValue;
	bar.m_maxValue = maxValue;
	bar.m_incrementSize = incrementSize;

	bar.m_pTexture = pTexture;
	bar.m_pTexture->AddRef();
	bar.m_uv0 = uv0;
	bar.m_uv1 = uv1;

	const int lenUnicode = MultiByteToWideChar(CP_UTF8, 0, pszName, -1, nullptr, 0);
	WCHAR* pszText = static_cast<WCHAR*>(TB8_MALLOC(lenUnicode * sizeof(WCHAR)));
	MultiByteToWideChar(CP_UTF8, 0, pszName, -1, pszText, lenUnicode);

	hr = m_pRenderer->GetWriteFactory()->CreateTextLayout(
		pszText,										// [in]  const WCHAR*			string,
		lenUnicode,										// UINT32						stringLength,
		m_pRenderer->GetFont(RenderFontID_StatusBar),	// IDWriteTextFormat * textFormat,
		4096.f,											// FLOAT              maxWidth,
		128.f,											// FLOAT              maxHeight,
		&(bar.m_pLayout));								// [out]       IDWriteTextLayout ** textLayout
	assert(hr == S_OK);

	// determine the size.
	DWRITE_TEXT_METRICS textMetrics;
	hr = bar.m_pLayout->GetMetrics(&textMetrics);
	assert(hr == S_OK);

	bar.m_textSize = Vector2(static_cast<f32>(static_cast<s32>(textMetrics.width + 0.99f)), static_cast<f32>(static_cast<s32>(textMetrics.height + 0.99f)));
	
	// allocate whole model for bar.
	bar.m_pModel_Whole = RenderModel::AllocSimpleRectangle(m_pRenderer, RenderMainViewType_UI, Vector3(1.f, 0.f, 0.f), Vector3(0.f, 1.f, 0.f), pTexture, uv0, uv1);

	// create partial model for bar.
	__CreatePartialModel(bar);

	// recompute sizes.
	__ComputeSizes();

	return bar.m_barID;
}

void RenderStatusBars::SetBarValue(u32 barID, s32 value)
{
	RenderStatusBars_Bar& bar = m_bars[barID - 1];
	value = std::min<s32>(value, bar.m_maxValue);
	value = std::max<s32>(value, 0);
	if (bar.m_value == value)
		return;

	// update the value.
	bar.m_value = value;

	// re-create partial model.
	__CreatePartialModel(bar);
}

void RenderStatusBars::SetBarValueDelta(u32 barID, s32 delta)
{
	RenderStatusBars_Bar& bar = m_bars[barID - 1];
	SetBarValue(barID, bar.m_value + delta);
}

void RenderStatusBars::Render3D()
{
	Vector2 pos(m_pRenderer->GetRenderScreenSize().x - m_spacingSize.x - m_barSize.x, m_spacingSize.y);
	const f32 inset = (m_textSize.y - m_barSize.y) / 2.f;

	for (std::vector<RenderStatusBars_Bar>::iterator it = m_bars.begin(); it != m_bars.end(); ++it)
	{
		RenderStatusBars_Bar& bar = *it;

		const f32 size = m_barSize.y;
		Matrix4 matrixScale;
		matrixScale.SetScale(Vector3(size, size, 0.f));

		// draw whole models.
		const s32 countWhole = bar.m_value / bar.m_incrementSize;
		for (s32 i = 0; i < countWhole; ++i)
		{
			Matrix4 matrixTranslate;
			matrixTranslate.SetTranslation(Vector3(pos.x + (i * size), pos.y + inset, 0.f));

			Matrix4 modelToWorld = matrixTranslate;
			modelToWorld = Matrix4::MultiplyAB(modelToWorld, matrixScale);

			bar.m_pModel_Whole->SetWorldTransform(modelToWorld);
			bar.m_pModel_Whole->Render(m_pRenderer);
		}

		// draw partial model.
		if ((countWhole * bar.m_incrementSize) < bar.m_value)
		{
			Matrix4 matrixTranslate;
			matrixTranslate.SetTranslation(Vector3(pos.x + (countWhole * size), pos.y + inset, 0.f));

			Matrix4 modelToWorld = matrixTranslate;
			modelToWorld = Matrix4::MultiplyAB(modelToWorld, matrixScale);

			bar.m_pModel_Partial->SetWorldTransform(modelToWorld);
			bar.m_pModel_Partial->Render(m_pRenderer);
		}

		pos.y += m_spacingSize.y + m_textSize.y;
	}
}

void RenderStatusBars::Render2D()
{
	HRESULT hr = S_OK;

	ID2D1SolidColorBrush* pWhiteBrush = nullptr;
	hr = m_pRenderer->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pWhiteBrush);
	assert(SUCCEEDED(hr));

	Vector2 pos(m_pRenderer->GetRenderScreenSize().x - m_spacingSize.x - m_barSize.x - m_spacingSize.x - m_textSize.x, m_spacingSize.y);
	for (std::vector<RenderStatusBars_Bar>::iterator it = m_bars.begin(); it != m_bars.end(); ++it)
	{
		RenderStatusBars_Bar& bar = *it;

		f32 insetY = (m_textSize.y - bar.m_textSize.y) / 2.f;

		m_pRenderer->GetD2DDeviceContext()->DrawTextLayout(
			D2D1::Point2F(pos.x, pos.y + insetY),	// D2D1_POINT_2F          origin,
			bar.m_pLayout,					// [in] IDWriteTextLayout      *textLayout,
			pWhiteBrush,					// [in] ID2D1Brush             *defaultForegroundBrush,
			D2D1_DRAW_TEXT_OPTIONS_NONE);	// D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE

		pos.y += m_spacingSize.y + m_textSize.y;
	}

	RELEASEI(pWhiteBrush);
}

void RenderStatusBars::__CreatePartialModel(RenderStatusBars_Bar& bar)
{
	RELEASEI(bar.m_pModel_Partial);

	const s32 countWhole = bar.m_value / bar.m_incrementSize;
	if ((countWhole * bar.m_incrementSize) == bar.m_value)
		return;

	const s32 remainder = bar.m_value - (countWhole * bar.m_incrementSize);
	const f32 portion = static_cast<f32>(remainder) / static_cast<f32>(bar.m_incrementSize);

	// allocate whole model for bar.
	bar.m_pModel_Partial = RenderModel::AllocSimpleRectangle(m_pRenderer, RenderMainViewType_UI, 
		Vector3(portion, 0.f, 0.f), Vector3(0.f, 1.f, 0.f),
		bar.m_pTexture, bar.m_uv0, Vector2(bar.m_uv1.x * portion, bar.m_uv1.y));
}

void RenderStatusBars::__ComputeSizes()
{
	const f32 spacing = static_cast<f32>(ComputeViewPixelsFromHardPixels(BASE_SPACING_PIXELS, m_pRenderer->GetRenderScaleSize()));
	m_spacingSize.x = spacing;
	m_spacingSize.y = spacing;

	m_textSize.x = 0.f;
	m_textSize.y = 0.f;

	s32 maxValue = 0;

	for (std::vector<RenderStatusBars_Bar>::iterator it = m_bars.begin(); it != m_bars.end(); ++it)
	{
		RenderStatusBars_Bar& bar = *it;
		m_textSize.x = std::max<f32>(m_textSize.x, bar.m_textSize.x);
		m_textSize.y = std::max<f32>(m_textSize.y, bar.m_textSize.y);

		maxValue = std::max<s32>(maxValue, bar.m_maxValue / bar.m_incrementSize);
	}

	m_barSize.y = static_cast<f32>(ComputeViewPixelsFromHardPixels(BASE_BAR_PIXELS, m_pRenderer->GetRenderScaleSize()));
	m_barSize.x = m_barSize.y * maxValue;

	m_textSize.y = std::max<f32>(m_textSize.y, m_barSize.y);
}

}
#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <map>

#include "common/basic_types.h"
#include "common/ref_count.h"

namespace TB8
{

class RenderMain;
class RenderTexture;
class RenderModel;

struct RenderStatusBars_Bar
{
	u32						m_barID;
	IDWriteTextLayout*		m_pLayout;
	Vector2					m_textSize;
	RenderTexture*			m_pTexture;
	Vector2					m_uv0;
	Vector2					m_uv1;
	RenderModel*			m_pModel_Whole;
	RenderModel*			m_pModel_Partial;
	s32						m_value;
	s32						m_maxValue;
	s32						m_incrementSize;
};

class RenderStatusBars : public ref_count
{
public:
	static RenderStatusBars* Alloc(RenderMain* pRenderer);

	RenderStatusBars(RenderMain* pRenderer);
	~RenderStatusBars();

	u32 AddBar(const char* pszName, RenderTexture* pTexture, const Vector2& uv0, const Vector2& uv1, s32 initialValue, s32 maxValue, s32 incrementSize);
	s32 GetBarValue(u32 barID) const { return m_bars[barID - 1].m_value; }
	void SetBarValue(u32 barID, s32 value);
	void SetBarValueDelta(u32 barID, s32 delta);
	void RemoveBar(u32 barID);

	void Render2D();
	void Render3D();

protected:
	void __Initialize();
	void __Uninitialize();
	virtual void __Free() override;
	void __CreatePartialModel(RenderStatusBars_Bar& bar);
	void __ComputeSizes();

	RenderMain*								m_pRenderer;
	std::vector<RenderStatusBars_Bar>		m_bars;

	Vector2									m_textSize;
	Vector2									m_barSize;
	Vector2									m_spacingSize;
};

}

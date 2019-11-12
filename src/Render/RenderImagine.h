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

struct RenderImagine_TextLine
{
	u32						m_index;
	IDWriteTextLayout*		m_pLayout;
	Vector2					m_size;
	Vector2					m_offsetPosToUL;
};

struct RenderImagine_Circle
{
	Vector2					m_offsetPosToCenter;
	f32						m_radius;
};

enum RenderImagine_Type : u32
{
	RenderImagine_Type_Imagine,
	RenderImagine_Type_Speech,
};

class RenderImagine : public ref_count
{
public:
	static RenderImagine* Alloc(RenderMain* pRenderer);

	RenderImagine(RenderMain* pRenderer);
	~RenderImagine();

	void SetType(RenderImagine_Type type) { m_type = type; }
	void SetPosition(const Vector2& pos);
	void SetText(const char* pszText);
	void SetImage(RenderTexture* pTexture);

	void Render();

protected:
	void __Initialize();
	virtual void __Free() override;
	void __ComputeLayout_Text();
	void __ComputeGeometry();
	void __ComputeImagineGeometry();
	void __ComputeSpeechGeometry();
	void __CleanupLayouts();
	void __AddBubblePoints();
	void __AdjustBubblePointSegments();
	void __AdjustBubblePointSpacing();
	void __AdjustBubblePointConvex();
	void __AddCircles();

	RenderMain*								m_pRenderer;

	RenderImagine_Type						m_type;
	WCHAR*									m_pszText;
	std::vector<RenderImagine_TextLine>		m_layouts;
	std::vector<RenderImagine_Circle>		m_circles;
	Vector2									m_posToCenter;
	Vector2									m_textSize;
	std::vector<Vector2>					m_bubblePoints;
	ID2D1PathGeometry*						m_pBubbleGeometry;

	Vector2									m_pos;
};

}

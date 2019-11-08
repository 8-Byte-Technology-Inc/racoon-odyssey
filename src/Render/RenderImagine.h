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


class RenderImagine : public ref_count
{
public:
	static RenderImagine* Alloc(RenderMain* pRenderer);

	RenderImagine(RenderMain* pRenderer);
	~RenderImagine();

	void SetSize(const IVector2& size);
	void SetPosition(const IVector2& pos);
	void SetText(const char* pszText);
	void SetImage(RenderTexture* pTexture);

	void Render();

protected:
	void __Initialize();
	virtual void __Free() override;

	RenderMain*			m_pRenderer;

	IVector2			m_size;
	IVector2			m_pos;

};

}

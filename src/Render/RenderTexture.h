#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "common/basic_types.h"
#include "common/ref_count.h"

namespace TB8
{

class RenderMain;

struct RenderTexture_MemoryTexture
{
	IVector2									m_size;
	u8*											m_data;
};

struct RenderTexture_SrcTextureInfo
{
	IVector2									m_srcSize;
	IRect										m_dstOuter;
	IRect										m_dstInner;
};

class RenderTexture : public ref_count
{
public:
	RenderTexture();
	~RenderTexture();

	static RenderTexture* Alloc(RenderMain* renderer, const char* pTexturePath);
	static RenderTexture* Alloc(RenderMain* renderer, const std::vector<RenderTexture_MemoryTexture>& memoryTextures, const std::vector<std::string>& texturePaths);

	void MapUV(s32 textureIndex, const Vector2& uvIn, Vector2* uvOut);

	ID3D11ShaderResourceView* GetTexture() { return m_pTexture; }

private:
	void __Initialize(RenderMain* renderer, const std::vector<RenderTexture_MemoryTexture>& memoryTextures, const std::vector<std::string>& texturePaths);
	virtual void __Free() override;

	ID3D11ShaderResourceView*					m_pTexture;
	IVector2									m_textureSize;
	std::vector<RenderTexture_SrcTextureInfo>	m_srcTextureInfo;
};

}

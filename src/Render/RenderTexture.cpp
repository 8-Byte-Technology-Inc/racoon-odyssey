#include "pch.h"

#include "RenderMain.h"

#include "RenderHelper.h"
#include "RenderTexture.h"

namespace TB8
{

RenderTexture::RenderTexture()
	: ref_count()
	, m_pTexture(nullptr)
{
}

RenderTexture::~RenderTexture()
{
	RELEASEI(m_pTexture);
}

/* static */ RenderTexture* RenderTexture::Alloc(RenderMain* renderer, const char* pTexturePath)
{
	RenderTexture* obj = TB8_NEW(RenderTexture)();
	std::vector<RenderTexture_MemoryTexture> memoryTextures;
	std::vector<std::string> texturePaths;
	texturePaths.push_back(std::string(pTexturePath));
	obj->__Initialize(renderer, memoryTextures, texturePaths);
	return obj;
}

/* static */ RenderTexture* RenderTexture::Alloc(RenderMain* renderer, const std::vector<RenderTexture_MemoryTexture>& memoryTextures, const std::vector<std::string>& texturePaths)
{
	RenderTexture* obj = TB8_NEW(RenderTexture)();
	obj->__Initialize(renderer, memoryTextures, texturePaths);
	return obj;
}

void RenderTexture::MapUV(s32 textureIndex, const Vector2& uvIn, Vector2* uvOut)
{
	RenderTexture_SrcTextureInfo& srcTextureInfo = m_srcTextureInfo[textureIndex];

	const f32 localTextureSizeX = static_cast<f32>(srcTextureInfo.m_srcSize.x);
	const f32 localTextureSizeY = static_cast<f32>(srcTextureInfo.m_srcSize.y);

	const s32 localPixelX = std::max<s32>(std::min<s32>(static_cast<s32>(uvIn.x * localTextureSizeX + 0.5f), srcTextureInfo.m_srcSize.x), 0);
	const s32 localPixelY = std::max<s32>(std::min<s32>(static_cast<s32>(uvIn.y * localTextureSizeY + 0.5f), srcTextureInfo.m_srcSize.y), 0);

	const s32 globalPixelX = localPixelX + srcTextureInfo.m_dstInner.left;
	const s32 globalPixelY = localPixelY + srcTextureInfo.m_dstInner.top;

	uvOut->x = static_cast<f32>(globalPixelX) / static_cast<f32>(m_textureSize.x);
	uvOut->y = static_cast<f32>(globalPixelY) / static_cast<f32>(m_textureSize.y);
}

void RenderTexture::__Free()
{
	TB8_DEL(this);
}

void RenderTexture::__Initialize(RenderMain* renderer, const std::vector<RenderTexture_MemoryTexture>& memoryTextures, const std::vector<std::string>& texturePaths)
{
	HRESULT hr = S_OK;
	IWICImagingFactory*	pWICFactory = renderer->GetWICFactory();

	// load the file up and create the decoder.
	std::vector<IWICFormatConverter*> converters;
	m_srcTextureInfo.reserve(texturePaths.size());
	converters.reserve(texturePaths.size());
	for (std::vector<std::string>::const_iterator it = texturePaths.begin(); it != texturePaths.end(); ++it)
	{
		WCHAR wPath[MAX_PATH];
		mbstowcs_s(nullptr, wPath, (*it).c_str(), ARRAYSIZE(wPath));
		IWICBitmapDecoder *pDecoder = nullptr;
		hr = pWICFactory->CreateDecoderFromFilename(
			wPath,
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			&pDecoder
		);
		assert(hr == S_OK);

		// get the first frame.
		IWICBitmapFrameDecode *pSource = nullptr;
		hr = pDecoder->GetFrame(0, &pSource);
		assert(hr == S_OK);

		// create the converter.
		IWICFormatConverter *pConverter = nullptr;
		hr = pWICFactory->CreateFormatConverter(&pConverter);
		assert(hr == S_OK);

		// init the converter.
		hr = pConverter->Initialize(
			pSource,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.f,
			WICBitmapPaletteTypeCustom
		);

		// determine size.
		UINT tempWidth, tempHeight;
		hr = pConverter->GetSize(&tempWidth, &tempHeight);

		const IVector2 buffer(tempWidth / 8, tempHeight / 8);

		RenderTexture_SrcTextureInfo srcTextureInfo;
		srcTextureInfo.m_srcSize = IVector2(tempWidth, tempHeight);
		srcTextureInfo.m_dstOuter = IRect(m_textureSize.x, 0, m_textureSize.x + buffer.x + tempWidth + buffer.x, 0 + buffer.y + tempHeight + buffer.y);
		srcTextureInfo.m_dstInner = IRect(srcTextureInfo.m_dstOuter.left + buffer.x, srcTextureInfo.m_dstOuter.top + buffer.y,
											srcTextureInfo.m_dstOuter.left + buffer.x + tempWidth, srcTextureInfo.m_dstOuter.top + buffer.y + tempHeight);
		m_srcTextureInfo.push_back(srcTextureInfo);

		m_textureSize.x += srcTextureInfo.m_dstOuter.Width();
		m_textureSize.y = std::max<s32>(m_textureSize.y, srcTextureInfo.m_dstOuter.Height());

		converters.push_back(pConverter);

		RELEASEI(pSource);
		RELEASEI(pDecoder);
	}

	// allocate a buffer.
	u32 cbStride = (((m_textureSize.x * 4) + 7) / 8) * 8;
	u32 cbBufferSize = cbStride * m_textureSize.y;
	u8* pBuffer = reinterpret_cast<u8*>(malloc(cbBufferSize));
	ZeroMemory(pBuffer, cbBufferSize);

	std::vector<RenderTexture_SrcTextureInfo>::iterator itSrcTextureInfo = m_srcTextureInfo.begin();

	// textures from files.
	for (std::vector<IWICFormatConverter*>::iterator itConverter = converters.begin(); itConverter != converters.end(); ++itConverter, ++itSrcTextureInfo)
	{
		IWICFormatConverter *pConverter = *itConverter;
		RenderTexture_SrcTextureInfo& srcTextureInfo = *itSrcTextureInfo;

		UINT cbDstOffset = (srcTextureInfo.m_dstInner.top * cbStride) + (srcTextureInfo.m_dstInner.left * 4);
		UINT cbDstBufferSize = cbBufferSize - cbDstOffset;
		u8* pDstBuffer = pBuffer + cbDstOffset;

		// copy pixels to our buffer.
		hr = pConverter->CopyPixels(
			nullptr,					// [in, unique] const WICRect *prc,
			cbStride,					// [in]               UINT    cbStride,
			cbDstBufferSize,			// [in]               UINT    cbBufferSize,
			pDstBuffer					// [out]              BYTE    *pbBuffer
		);
		assert(hr == S_OK);

		RELEASEI(pConverter);

		// fill in the buffer area by repeating portions of the source structure such that it wraps around.
		IVector2 dstPixelGlobal(srcTextureInfo.m_dstOuter.left, srcTextureInfo.m_dstOuter.top);
		for (; dstPixelGlobal.y < srcTextureInfo.m_dstOuter.bottom; ++dstPixelGlobal.y, dstPixelGlobal.x = srcTextureInfo.m_dstOuter.left)
		{
			for (; dstPixelGlobal.x < srcTextureInfo.m_dstOuter.right; ++dstPixelGlobal.x)
			{
				if (!srcTextureInfo.m_dstInner.Intersect(dstPixelGlobal))
				{
					const IVector2 srcPixelLocal(dstPixelGlobal.x - srcTextureInfo.m_dstInner.left, dstPixelGlobal.y - srcTextureInfo.m_dstInner.top);
					const IVector2 srcPixelGlobal(((srcPixelLocal.x + srcTextureInfo.m_srcSize.x) % srcTextureInfo.m_srcSize.x) + srcTextureInfo.m_dstInner.left,
													((srcPixelLocal.y + srcTextureInfo.m_srcSize.y) % srcTextureInfo.m_srcSize.y) + srcTextureInfo.m_dstInner.top);
					const u32* pixelSrc = reinterpret_cast<u32*>(pBuffer + (srcPixelGlobal.y * cbStride) + (srcPixelGlobal.x * 4));
					u32* pixelDst = reinterpret_cast<u32*>(pBuffer + (dstPixelGlobal.y * cbStride) + (dstPixelGlobal.x * 4));
					*pixelDst = *pixelSrc;
				}
			}
		}
	}
	converters.clear();

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = m_textureSize.x;
	desc.Height = m_textureSize.y;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = pBuffer;
	initialData.SysMemPitch = static_cast<UINT>(cbStride);
	initialData.SysMemSlicePitch = static_cast<UINT>(cbBufferSize);

	ID3D11Texture2D* pTexture = nullptr;
	hr = renderer->GetDevice()->CreateTexture2D(&desc, &initialData, &pTexture);
	assert(hr == S_OK);

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	memset(&SRVDesc, 0, sizeof(SRVDesc));
	SRVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	assert(m_pTexture == nullptr);
	hr = renderer->GetDevice()->CreateShaderResourceView(pTexture, &SRVDesc, &m_pTexture);
	assert(hr == S_OK);

	// generate mipmaps.
	renderer->GetDeviceContext()->GenerateMips(m_pTexture);
	
	RELEASEI(pTexture);
	PTRFREE(pBuffer);
}

}

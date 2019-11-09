#pragma once

#include <vector>

#include "client/client_globals.h"

namespace TB8
{

class RenderModelGroup;
class RenderShader;
enum RenderShaderID : u32;
enum RenderScaleSize : u32;

struct RenderMain_MutableData
{
	bool					m_isChanged;

	s32						m_dpi;
	RenderScaleSize			m_scaleSize;
	IVector2				m_screenSizePixels;
	Vector2					m_screenSizeWorld;
	IVector2				m_mousePos;

	RenderMain_MutableData();
};

enum RenderFontID : u32
{
	RenderFontID_Unknown = 0,

	RenderFontID_Imagine = 1,

	RenderFontID_COUNT,

	RenderFontID_Custom
};

enum RenderFontDirection : u32
{
	RenderFontDirection_LeftToRight = 0,
	RenderFontDirection_TopToBottom,
	RenderFontDirection_BottomToTop,
};

struct RenderMainFont
{
	RenderFontID				m_fontID;
	std::string					m_name;
	std::string					m_type;
	s32							m_size;
	s32							m_weight;
	RenderFontDirection			m_direction;
	IDWriteTextFormat*			m_pFont;
};

class RenderMain : public Client_Globals_Accessor
{
public:
	RenderMain(HWND hWnd, Client_Globals* pGlobalState);
	~RenderMain();

	static RenderMain* Alloc(HWND hWnd, Client_Globals* pClientGlobals);
	void Free();

	void UpdateRender(s32 frameCount);

	float GetAspectRatio();
	s32 GetWidth();
	s32 GetHeight();

	ID3D11Device*           GetDevice() { return m_pD3DDevice; };
	ID3D11DeviceContext*    GetDeviceContext() { return m_pD3DDeviceContext; };
	ID3D11RenderTargetView* GetRenderTarget() { return m_pRenderTarget; }
	ID3D11DepthStencilView* GetDepthStencil() { return m_pDepthStencilView; }
	IWICImagingFactory*		GetWICFactory() { return m_wicFactory; }
	ID2D1Factory1*			GetD2DFactory() { return m_pD2DFactory; }
	ID2D1DeviceContext*		GetD2DDeviceContext() { return m_pD2DDeviceContext; }
	IDWriteFactory*			GetWriteFactory() { return m_pDWriteFactory; }

	void BeginUpdate();
	void EndUpdate();
	void BeginDraw();
	void EndDraw();

	void SetViewMatrix(const Matrix4& viewMatrix);
	void SetProjectionMatrix(const Matrix4& projectionMatrix);
	void SetLightVector(const Vector3& lightVector);

	RenderShader*	GetShaderByID(RenderShaderID shaderID);

	const Matrix4&	GetViewMatrix() const { return m_viewMatrix; }
	const Matrix4&	GetProjectionMatrix() const { return m_projectionMatrix; }

	void			SetBGColor(u32 rgba);

	const IVector2& GetRenderMousePos() const { return m_dataRender.m_mousePos; }
	void SetRenderMousePos(const IVector2& pos);
	RenderScaleSize GetRenderScaleSize() const { return m_dataRender.m_scaleSize; }
	const IVector2& GetRenderScreenSize() const { return m_dataRender.m_screenSizePixels; }
	s32 GetRenderDPI() const { return m_dataRender.m_dpi; }
	const Vector2& GetRenderScreenSizeWorld() const { return m_dataRender.m_screenSizeWorld; }
	void AlignWorldPosition(Vector3& worldPos);
	void AlignWorldSize(Vector3& worldSize);
	void AlignScreenSize(IVector2& screenSize);
	Vector2 WorldToScreenCoords(const Vector3& worldCoords);

	void DrawCursor();
	bool IsCursorVisible() const;

	IDWriteTextFormat* GetFont(RenderFontID fontID) const { return m_fonts[fontID].m_pFont; }

private:
	void __Initialize();
	void __Shutdown();

	void __InitFonts();
	void __CreateFont(RenderMainFont& fontInfo);
	void __ClearFonts();
	void __CreateFonts();

	void __ConfigureBuffers();
	void __ConfigureBuffers_D3D();
	void __ConfigureBuffers_D2D();
	void __ReleaseBuffers();
	void __ResizeBuffers();
	void __SetScreenSizePixels(const IVector2& size, s32 dpi);
	void __ConfigureTransforms();

	void __EventHandler(EventMessage* message);
	void __EventResizeWindow(class EventMessage_ResizeWindow* event);

	HWND						m_hWnd;

	IWICImagingFactory*			m_wicFactory;

	ID3D11Device*				m_pD3DDevice;
	ID3D11DeviceContext*		m_pD3DDeviceContext;

	IDXGIDevice1*				m_pDXGIDevice;
	IDXGIFactory2*				m_pDXGIFactory;
	IDXGISwapChain1*			m_pDXGISwapChain;

	ID2D1Factory1*				m_pD2DFactory;
	ID2D1Device*				m_pD2DDevice;
	ID2D1DeviceContext*			m_pD2DDeviceContext;
	ID2D1Bitmap1*				m_pD2DScreenBitmap;
	IDWriteFactory*				m_pDWriteFactory;

	ID3D11Texture2D*			m_pBackBuffer;
	ID3D11RenderTargetView*		m_pRenderTarget;
	D3D11_TEXTURE2D_DESC		m_bbDesc;
	ID3D11Texture2D*			m_pDepthStencilBuffer;
	ID3D11DepthStencilState*	m_pDepthStencilState;
	ID3D11DepthStencilView*		m_pDepthStencilView;
	ID3D11RasterizerState*		m_pRasterizerState;

	Matrix4						m_projectionMatrix;
	Matrix4						m_viewMatrix;
	Vector3						m_lightVector;

	Matrix4						m_worldToScreen;
	Matrix4						m_screenToWorld;

	float						m_bgColor[4];

	std::vector<RenderShader*>	m_shaders;
	std::vector<RenderMainFont>		m_fonts;

	RenderMain_MutableData		m_dataRender;
};

}
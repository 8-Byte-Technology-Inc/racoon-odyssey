#include "pch.h"

#include "Event/EventQueue.h"
#include "Event/EventQueueMessages.h"

#include "RenderHelper.h"
#include "RenderScale.h"

#include "RenderShaders.h"
#include "RenderMain.h"

namespace TB8
{

RenderMain_MutableData::RenderMain_MutableData()
	: m_isChanged(false)
	, m_dpi(96)
	, m_scaleSize(RenderScaleSize_100)
{
}

RenderMain::RenderMain(HWND hWnd, Client_Globals* pGlobalState)
	: Client_Globals_Accessor(pGlobalState)
	, m_hWnd(hWnd)
	, m_wicFactory(nullptr)
	, m_pD3DDevice(nullptr)
	, m_pD3DDeviceContext(nullptr)
	, m_pDXGIDevice(nullptr)
	, m_pDXGIFactory(nullptr)
	, m_pDXGISwapChain(nullptr)
	, m_pD2DFactory(nullptr)
	, m_pD2DDevice(nullptr)
	, m_pD2DDeviceContext(nullptr)
	, m_pD2DScreenBitmap(nullptr)
	, m_pDWriteFactory(nullptr)
	, m_pBackBuffer(nullptr)
	, m_pRenderTarget(nullptr)
	, m_pDepthStencilBuffer(nullptr)
	, m_pDepthStencilState(nullptr)
	, m_pDepthStencilView(nullptr)
	, m_pRasterizerState(nullptr)
	, m_pBlendState(nullptr)
	, m_lightVector(0.f, -1.f, 0.f)
{
	// default, unless overridden.
	m_bgColor[0] = 0.000f;
	m_bgColor[1] = 0.500f;
	m_bgColor[2] = 0.500f;
	m_bgColor[3] = 1.000f;
}

RenderMain::~RenderMain()
{
	__Shutdown();
}

/* static */ RenderMain* RenderMain::Alloc(HWND hWnd, Client_Globals* pGlobalState)
{
	RenderMain* obj = new RenderMain(hWnd, pGlobalState);
	obj->__Initialize();
	return obj;
}

void RenderMain::Free()
{
	delete this;
}

void RenderMain::UpdateRender(s32 frameCount)
{

}

RenderShader* RenderMain::GetShaderByID(RenderShaderID shaderID)
{
	return m_shaders[shaderID - 1];
}

void RenderMain::SetViewMatrix(RenderMainViewType t, const Matrix4& viewMatrix, const Matrix4& projectionMatrix)
{
	assert(t < m_views.size());

	RenderMainView& view = m_views[t];
	view.m_viewMatrix = viewMatrix;
	view.m_projectionMatrix = projectionMatrix;
	view.m_pConstantBuffer->SetView(view.m_viewMatrix, view.m_projectionMatrix);
}

void RenderMain::SetLightVector(const Vector3& lightVector)
{
	if (m_lightVector == lightVector)
		return;

	m_lightVector = lightVector;

	for (std::vector<RenderShader*>::iterator it = m_shaders.begin(); it != m_shaders.end(); ++it)
	{
		RenderShader* pShader = *it;
		if (pShader)
		{
			pShader->SetLightVector(m_lightVector);
		}
	}
}

void RenderMain::SetBGColor(u32 rgba)
{
	m_bgColor[0] = static_cast<f32>((rgba >> 24) & 0xff) / 255.0f;
	m_bgColor[1] = static_cast<f32>((rgba >> 16) & 0xff) / 255.0f;
	m_bgColor[2] = static_cast<f32>((rgba >> 8) & 0xff) / 255.0f;
	m_bgColor[3] = static_cast<f32>((rgba >> 0) & 0xff) / 255.0f;
}

void RenderMain::SetRenderMousePos(const IVector2& pos)
{
	if (m_dataRender.m_mousePos == pos)
		return;

	m_dataRender.m_mousePos = pos;
	m_dataRender.m_isChanged = true;
}

bool RenderMain::IsCursorVisible() const
{
	return false;
}

void RenderMain::DrawCursor()
{
}

void RenderMain::__Shutdown()
{
	for (std::vector<RenderShader*>::iterator it = m_shaders.begin(); it != m_shaders.end(); ++it)
	{
		RELEASEI(*it);
	}
	m_shaders.clear();
	__ClearFonts();

	RELEASEI(m_pBlendState);
	RELEASEI(m_pRasterizerState);
	RELEASEI(m_pDepthStencilView);
	RELEASEI(m_pDepthStencilState);
	RELEASEI(m_pDepthStencilBuffer);
	RELEASEI(m_pRenderTarget);
	RELEASEI(m_pBackBuffer);

	RELEASEI(m_pD2DScreenBitmap);

	RELEASEI(m_pDWriteFactory);
	RELEASEI(m_pDXGISwapChain);
	RELEASEI(m_pDXGIFactory);

	RELEASEI(m_wicFactory);
	RELEASEI(m_pD2DDeviceContext);
	RELEASEI(m_pD2DDevice);
	RELEASEI(m_pDXGIDevice);

	RELEASEI(m_pD3DDeviceContext);
	RELEASEI(m_pD3DDevice);

	RELEASEI(m_pD2DFactory);
}

void RenderMain::__Initialize()
{
	HRESULT hr = S_OK;

	assert(m_pD2DFactory == nullptr);
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr, reinterpret_cast<void**>(&m_pD2DFactory));
	assert(hr == S_OK);

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
//		D3D_FEATURE_LEVEL_9_3,
//		D3D_FEATURE_LEVEL_9_2,
//		D3D_FEATURE_LEVEL_9_1
	};

	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(DEBUG) || defined(_DEBUG)
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	assert(m_pD3DDevice == nullptr);
	assert(m_pD3DDeviceContext == nullptr);
	hr = D3D11CreateDevice(
		nullptr,						// IDXGIAdapter            *pAdapter,
		D3D_DRIVER_TYPE_HARDWARE,		// D3D_DRIVER_TYPE         DriverType,
		nullptr,						// HMODULE                 Software,
		creationFlags,					// UINT                    Flags,
		featureLevels,					// const D3D_FEATURE_LEVEL *pFeatureLevels,
		ARRAYSIZE(featureLevels),		// UINT                    FeatureLevels,
		D3D11_SDK_VERSION,				// UINT                    SDKVersion,
		&m_pD3DDevice,					// ID3D11Device            **ppDevice,
		nullptr,						// D3D_FEATURE_LEVEL       *pFeatureLevel,
		&m_pD3DDeviceContext			// ID3D11DeviceContext     **ppImmediateContext
	);
	assert(hr == S_OK);

	assert(m_pDXGIDevice == nullptr);
	hr = m_pD3DDevice->QueryInterface(&m_pDXGIDevice);
	assert(hr == S_OK);

	assert(m_pD2DDevice == nullptr);
	hr = m_pD2DFactory->CreateDevice(m_pDXGIDevice, &m_pD2DDevice);
	assert(hr == S_OK);

	// Create a device context.
	assert(m_pD2DDeviceContext == nullptr);
	hr = m_pD2DDevice->CreateDeviceContext(
		D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
		&m_pD2DDeviceContext
	);
	assert(hr == S_OK);

	// Create a WIC Factory
	assert(m_wicFactory == nullptr);
	hr = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_IWICImagingFactory,
		(LPVOID*)&m_wicFactory);
	assert(hr == S_OK);

	RECT rc;
	GetClientRect(m_hWnd, &rc);
	__SetScreenSizePixels(IVector2(rc.right - rc.left, rc.bottom - rc.top), GetDpiForWindow(m_hWnd));

	// Identify the physical adapter (GPU or card) this device is runs on.
	IDXGIAdapter* dxgiAdapter = nullptr;
	hr = m_pDXGIDevice->GetAdapter(&dxgiAdapter);
	assert(hr == S_OK);

	// Get the factory object that created the DXGI device.
	assert(m_pDXGIFactory == nullptr);
	hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&m_pDXGIFactory));
	assert(hr == S_OK);

	// Ensure that DXGI doesn't queue more than one frame at a time.
	m_pDXGIDevice->SetMaximumFrameLatency(1);

	// Allocate a descriptor.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	swapChainDesc.Width = m_dataRender.m_screenSizePixels.x;
	swapChainDesc.Height = m_dataRender.m_screenSizePixels.y;
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// this is the most common swapchain format
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;								// don't use multi-sampling
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;									// use double buffering to enable flip
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// all apps must use this SwapEffect
	swapChainDesc.Flags = 0;

	// Get the final swap chain for this window from the DXGI factory.
	assert(m_pDXGISwapChain == nullptr);
	hr = m_pDXGIFactory->CreateSwapChainForHwnd(
		m_pDXGIDevice,
		m_hWnd,
		&swapChainDesc,						// DXGI_SWAP_CHAIN_DESC1
		nullptr,							// DXGI_SWAP_CHAIN_FULLSCREEN_DESC 
		nullptr,							// pRestrictToOutput
		&m_pDXGISwapChain
	);
	assert(hr == S_OK);

	// create the bitmap
	__ConfigureBuffers();

	assert(m_pDWriteFactory == nullptr);
	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&m_pDWriteFactory));
	assert(hr == S_OK);

	// register for events.
	__GetEventQueue()->RegisterForMessage(EventModuleID_Renderer, EventMessageID_ResizeWindow, std::bind(&RenderMain::__EventHandler, this, std::placeholders::_1));

	RELEASEI(dxgiAdapter);

	// create shaders.
	m_shaders.resize(1);
	m_shaders[0] = RenderShader::Alloc(this, RenderShaderID_Generic, __GetPathBinary().c_str(), "GenericVertexShader.cso", "GenericPixelShader.cso");

	// create fonts.
	__InitFonts();

	// configure transforms.
	__ConfigureViews();
}

struct RenderFontInfo
{
	RenderFontID		m_fontID;
	const char*			m_name;
	const char*			m_type;
	s32					m_size;
	s32					m_weight;
	RenderFontDirection	m_direction;
};

static RenderFontInfo s_renderFontInfo[] =
{
	{ RenderFontID_Imagine, "uif_imagine", "Verdana", 18, 400, RenderFontDirection_LeftToRight },
	{ RenderFontID_StatusBar, "uif_statusbar", "Verdana", 12, 400, RenderFontDirection_LeftToRight },
};

void RenderMain::__InitFonts()
{
	HRESULT hr = S_OK;
	m_fonts.resize(RenderFontID_COUNT);
	for (s32 i = 0; i < ARRAYSIZE(s_renderFontInfo); ++i)
	{
		const RenderFontInfo& fontInfoSrc = s_renderFontInfo[i];
		RenderMainFont& fontInfo = m_fonts[fontInfoSrc.m_fontID];
		fontInfo.m_fontID = fontInfoSrc.m_fontID;
		fontInfo.m_name = fontInfoSrc.m_name;
		fontInfo.m_type = fontInfoSrc.m_type;
		fontInfo.m_size = fontInfoSrc.m_size;
		fontInfo.m_weight = fontInfoSrc.m_weight;
		fontInfo.m_direction = fontInfoSrc.m_direction;
		__CreateFont(fontInfo);
	}
}

void RenderMain::__ClearFonts()
{
	for (std::vector<RenderMainFont>::iterator it = m_fonts.begin(); it != m_fonts.end(); ++it)
	{
		RenderMainFont& fontInfo = *it;
		RELEASEI(fontInfo.m_pFont);
	}
}

void RenderMain::__CreateFonts()
{
	for (std::vector<RenderMainFont>::iterator it = m_fonts.begin(); it != m_fonts.end(); ++it)
	{
		RenderMainFont& fontInfo = *it;
		if (fontInfo.m_fontID != RenderFontID_Unknown)
		{
			__CreateFont(fontInfo);
		}
	}
}

void RenderMain::__CreateFont(RenderMainFont& fontInfo)
{
	HRESULT hr = S_OK;

	// convert type into wide string.
	WCHAR wszFontFamily[256];
	mbstowcs_s(nullptr, wszFontFamily, fontInfo.m_type.c_str(), ARRAYSIZE(wszFontFamily));

	RELEASEI(fontInfo.m_pFont);
	hr = m_pDWriteFactory->CreateTextFormat(
		wszFontFamily,								// [in]  const WCHAR                 * fontFamilyName,
		nullptr,									// IDWriteFontCollection * fontCollection,
		(DWRITE_FONT_WEIGHT)(fontInfo.m_weight),	// DWRITE_FONT_WEIGHT     fontWeight,
		DWRITE_FONT_STYLE_NORMAL,					// DWRITE_FONT_STYLE      fontStyle,
		DWRITE_FONT_STRETCH_NORMAL,					// DWRITE_FONT_STRETCH    fontStretch,
		static_cast<FLOAT>(ComputeViewPixelsFromHardPixels(fontInfo.m_size, m_dataRender.m_scaleSize)), // FLOAT                  fontSize,
		L"enUS",									// [in] const WCHAR                 * localeName,
		&(fontInfo.m_pFont));						// [out] IDWriteTextFormat     ** textFormat
	assert(hr == S_OK);

	if (fontInfo.m_direction == RenderFontDirection_TopToBottom)
	{
		//fontInfo.m_pFont->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
		fontInfo.m_pFont->SetFlowDirection(DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
	}
	else  if (fontInfo.m_direction == RenderFontDirection_BottomToTop)
	{
		//fontInfo.m_pFont->SetReadingDirection(DWRITE_READING_DIRECTION_BOTTOM_TO_TOP);
		fontInfo.m_pFont->SetFlowDirection(DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP);
	}
}

void RenderMain::__ConfigureViews()
{
	m_views.resize(2);

	__ConfigureView_World();
	__ConfigureView_UI();

	// light source
	{
		const Vector3 lightVectorWorld(+0.25f, +1.0f, -0.25f);

		SetLightVector(lightVectorWorld);
	}
}

void RenderMain::__ConfigureView_World()
{
	RenderMainView& viewWorld = m_views[RenderMainViewType_World];

	// view transform; camera position around model.
	{
		Matrix4 matrixRotateX;
		matrixRotateX.SetRotateX(-DirectX::XM_PI / 8.f);

		Matrix4 matrixRotateZ;
		matrixRotateZ.SetRotateZ(DirectX::XM_PI / 8.f);

		viewWorld.m_viewMatrix = matrixRotateZ;
		viewWorld.m_viewMatrix = Matrix4::MultiplyAB(viewWorld.m_viewMatrix, matrixRotateX);
	}

	// change coordinate system from world -> directx.
	Matrix4 projectionCoords;
	{
		projectionCoords.SetIdentity();
		projectionCoords.m[1][1] = 0.0f;
		projectionCoords.m[1][2] = -1.0f;
		projectionCoords.m[2][2] = 0.0f;
		projectionCoords.m[2][1] = 1.0f;
	}

	// projection transform; world -> screen coordinates.
	Matrix4 projectionMatrix;
	{
		const f32 zNear = 1024.f;
		const f32 zFar = 5120.f;
		const f32 zMid = (zNear + zFar) / 2.f;

		// move 0.0 into the middle of z.
		Matrix4 projectionMoveZ;
		projectionMoveZ.SetTranslation(Vector3(0.f, 0.f, zMid - zNear));

		// meters -> directx.
		f32 scaleX = static_cast<f32>(m_dataRender.m_dpi * 4) / static_cast<f32>(m_dataRender.m_screenSizePixels.x);
		f32 scaleY = static_cast<f32>(m_dataRender.m_dpi * 4) / static_cast<f32>(m_dataRender.m_screenSizePixels.y);
		f32 scaleZ = 1.f / (zFar - zNear);
		Matrix4 projectionScale;
		projectionScale.SetScale(Vector3(scaleX, scaleY, scaleZ));

		viewWorld.m_projectionMatrix = projectionScale;
		viewWorld.m_projectionMatrix = Matrix4::MultiplyAB(viewWorld.m_projectionMatrix, projectionMoveZ);
		viewWorld.m_projectionMatrix = Matrix4::MultiplyAB(viewWorld.m_projectionMatrix, projectionCoords);
	}

	// create constant buffer.
	viewWorld.m_pConstantBuffer = RenderShader_ConstantBuffer::AllocView(this);

	// set these transforms.
	viewWorld.m_pConstantBuffer->SetView(viewWorld.m_viewMatrix, viewWorld.m_projectionMatrix);

	// world to screen.
	{
		Matrix4 matrixScale;
		matrixScale.SetScale(Vector3(static_cast<f32>(m_dataRender.m_dpi * 2), static_cast<f32>(m_dataRender.m_dpi * 2), 1.f));

		viewWorld.m_worldToScreen = matrixScale;
		viewWorld.m_worldToScreen = Matrix4::MultiplyAB(viewWorld.m_worldToScreen, projectionCoords);
		viewWorld.m_worldToScreen = Matrix4::MultiplyAB(viewWorld.m_worldToScreen, viewWorld.m_viewMatrix);
	}

	// screen to world.
	{
		Matrix4 matrixRotateX;
		matrixRotateX.SetRotateX(+DirectX::XM_PI / 8.f);

		Matrix4 matrixRotateZ;
		matrixRotateZ.SetRotateZ(-DirectX::XM_PI / 8.f);

		Matrix4 matrixCoords;
		matrixCoords.SetIdentity();
		matrixCoords.m[1][1] = 0.0f;
		matrixCoords.m[1][2] = +1.0f;
		matrixCoords.m[2][2] = 0.0f;
		matrixCoords.m[2][1] = -1.0f;

		Matrix4 matrixScale;
		matrixScale.SetScale(Vector3(1.f / static_cast<f32>(m_dataRender.m_dpi * 2), 1.f / static_cast<f32>(m_dataRender.m_dpi * 2), 1.f));

		viewWorld.m_screenToWorld = matrixRotateX;
		viewWorld.m_screenToWorld = Matrix4::MultiplyAB(viewWorld.m_screenToWorld, matrixRotateZ);
		viewWorld.m_screenToWorld = Matrix4::MultiplyAB(viewWorld.m_screenToWorld, matrixCoords);
		viewWorld.m_screenToWorld = Matrix4::MultiplyAB(viewWorld.m_screenToWorld, matrixScale);
	}
}

void RenderMain::__ConfigureView_UI()
{
	RenderMainView& viewUI = m_views[RenderMainViewType_UI];

	const f32 zNear = 1024.f;
	const f32 zFar = 5120.f;
	const f32 zMid = (zNear + zFar) / 2.f;

	// view transform; camera position around model.
	viewUI.m_viewMatrix.SetIdentity();

	// 0,0 is upper left in UI, but -x, -y in DirectX, also pull forward, the UI is in front of everything else.
	Matrix4 projectionTrans;
	projectionTrans.SetTranslation(Vector3(-static_cast<f32>(m_dataRender.m_screenSizePixels.x / 2), -static_cast<f32>(m_dataRender.m_screenSizePixels.y / 2), -1024.f));

	// change coordinate system from UI -> directx.
	Matrix4 projectionCoords;
	projectionCoords.SetIdentity();
	projectionCoords.m[1][1] = -1.0f;

	// move 0.0 into the middle of z.
	Matrix4 projectionMoveZ;
	projectionMoveZ.SetTranslation(Vector3(0.f, 0.f, zMid - zNear));

	// UI -> directx.
	f32 scaleX = 2.f / static_cast<f32>(m_dataRender.m_screenSizePixels.x);
	f32 scaleY = 2.f / static_cast<f32>(m_dataRender.m_screenSizePixels.y);
	f32 scaleZ = 1.f / (zFar - zNear);
	Matrix4 projectionScale;
	projectionScale.SetScale(Vector3(scaleX, scaleY, scaleZ));

	// projection transform; world -> screen coordinates.
	viewUI.m_projectionMatrix = projectionScale;
	viewUI.m_projectionMatrix = Matrix4::MultiplyAB(viewUI.m_projectionMatrix, projectionMoveZ);
	viewUI.m_projectionMatrix = Matrix4::MultiplyAB(viewUI.m_projectionMatrix, projectionCoords);
	viewUI.m_projectionMatrix = Matrix4::MultiplyAB(viewUI.m_projectionMatrix, projectionTrans);

	// create constant buffer.
	viewUI.m_pConstantBuffer = RenderShader_ConstantBuffer::AllocView(this);

	// set these transforms.
	viewUI.m_pConstantBuffer->SetView(viewUI.m_viewMatrix, viewUI.m_projectionMatrix);
}

void RenderMain::AlignWorldPosition(Vector3& worldPos)
{
	RenderMainView& viewWorld = m_views[RenderMainViewType_World];

	// world to screen.
	Vector3 screenPos = Matrix4::MultiplyVector(worldPos, viewWorld.m_worldToScreen);
	screenPos.x = static_cast<f32>(static_cast<s32>(screenPos.x)) + 0.5f;
	screenPos.y = static_cast<f32>(static_cast<s32>(screenPos.y)) + 0.5f;
	worldPos = Matrix4::MultiplyVector(screenPos, viewWorld.m_screenToWorld);
}

void RenderMain::AlignWorldSize(Vector3& worldSize)
{
	RenderMainView& viewWorld = m_views[RenderMainViewType_World];

	// world to screen.
	Vector3 screenPos = Matrix4::MultiplyVector(worldSize, viewWorld.m_worldToScreen);
	screenPos.x = static_cast<f32>(static_cast<s32>(screenPos.x + 0.5f));
	screenPos.y = static_cast<f32>(static_cast<s32>(screenPos.y + 0.5f));
	worldSize = Matrix4::MultiplyVector(screenPos, viewWorld.m_screenToWorld);
}

void RenderMain::AlignScreenSize(IVector2& screenSize)
{
	RenderMainView& viewWorld = m_views[RenderMainViewType_World];

	Vector3 screenPos(static_cast<f32>(screenSize.x), static_cast<f32>(screenSize.y), 0.f);
	Vector3 worldSize = Matrix4::MultiplyVector(screenPos, viewWorld.m_screenToWorld);
	worldSize.x = static_cast<f32>(static_cast<s32>((worldSize.x * 10.f) + 0.5f)) / 10.f;
	worldSize.y = static_cast<f32>(static_cast<s32>((worldSize.y * 10.f) + 0.5f)) / 10.f;
	worldSize.z = static_cast<f32>(static_cast<s32>((worldSize.z * 10.f) + 0.5f)) / 10.f;
	Vector3 screenSize2 = Matrix4::MultiplyVector(worldSize, viewWorld.m_worldToScreen);
	screenSize.x = static_cast<s32>(screenSize2.x);
	screenSize.y = static_cast<s32>(screenSize2.y);

	screenSize.x = ((screenSize.x + 1) / 2) * 2;
	screenSize.y = ((screenSize.y + 1) / 2) * 2;
}

Vector2 RenderMain::WorldToScreenCoords(const Vector3& worldCoords)
{
	RenderMainView& viewWorld = m_views[RenderMainViewType_World];

	// this is centered on the middle of the screen.
	const Vector3 screenCoordsDX = Matrix4::MultiplyVector(worldCoords, viewWorld.m_worldToScreen);

	// convert relative to UL.
	return Vector2(((static_cast<f32>(m_dataRender.m_screenSizePixels.x) / 2.f) + screenCoordsDX.x),
					(static_cast<f32>(m_dataRender.m_screenSizePixels.y) / 2.f) - screenCoordsDX.y);
}

void RenderMain::__SetScreenSizePixels(const IVector2& screenSizePixels, s32 dpi)
{
	m_dataRender.m_dpi = dpi;

	m_dataRender.m_scaleSize = GetRenderScaleFromDPI(dpi);

	m_dataRender.m_screenSizePixels = screenSizePixels;

	// world coordinates in meters, let 1 meter = 2 screen inches.
	const f32 pixelsPerMeter = static_cast<f32>(dpi) * 2.f;
	const f32 pixelSizeWorld = 1.f / pixelsPerMeter;

	m_dataRender.m_screenSizeWorld.x = static_cast<f32>(screenSizePixels.x) * pixelSizeWorld;
	m_dataRender.m_screenSizeWorld.y = static_cast<f32>(screenSizePixels.y) * pixelSizeWorld;
}

void RenderMain::__ConfigureBuffers()
{
	__ConfigureBuffers_D3D();
	__ConfigureBuffers_D2D();
}

void RenderMain::__ConfigureBuffers_D3D()
{
	HRESULT hr = S_OK;

	// get a pointer to the back buffer.
	assert(m_pBackBuffer == nullptr);
	hr = m_pDXGISwapChain->GetBuffer(
		0,
		__uuidof(ID3D11Texture2D),
		(void**)&m_pBackBuffer);
	assert(hr == S_OK);

	// get a pointer to the render target view.
	assert(m_pRenderTarget == nullptr);
	hr = m_pD3DDevice->CreateRenderTargetView(
		m_pBackBuffer,
		nullptr,
		&m_pRenderTarget
	);
	assert(hr == S_OK);

	m_pBackBuffer->GetDesc(&m_bbDesc);

	// Create a depth buffer.
	D3D11_TEXTURE2D_DESC depthBufferDesc;
	ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));
	depthBufferDesc.Width = m_bbDesc.Width;
	depthBufferDesc.Height = m_bbDesc.Height;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.MiscFlags = 0;

	assert(m_pDepthStencilBuffer == nullptr);
	hr = m_pD3DDevice->CreateTexture2D(
		&depthBufferDesc,
		nullptr,
		&m_pDepthStencilBuffer
	);
	assert(hr == S_OK);

	// Initialize the description of the stencil state.
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	// Set up the description of the stencil state.
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create the depth stencil state.
	assert(m_pDepthStencilState == nullptr);
	hr = m_pD3DDevice->CreateDepthStencilState(
		&depthStencilDesc,
		&m_pDepthStencilState
	);
	assert(hr == S_OK);

	// Set the depth stencil state.
	m_pD3DDeviceContext->OMSetDepthStencilState(m_pDepthStencilState, 1);

	// Initailze the depth stencil view.
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	// Set up the depth stencil view description.
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	hr = m_pD3DDevice->CreateDepthStencilView(
		m_pDepthStencilBuffer,
		&depthStencilViewDesc,
		&m_pDepthStencilView
	);
	assert(hr == S_OK);

	// Bind the render target view and depth stencil buffer to the output render pipeline.
	m_pD3DDeviceContext->OMSetRenderTargets(
		1,
		&m_pRenderTarget,
		m_pDepthStencilView
	);

	// Setup the raster description which will determine how and what polygons will be drawn.
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state from the description we just filled out.
	assert(m_pRasterizerState == nullptr);
	hr = m_pD3DDevice->CreateRasterizerState(
		&rasterDesc,
		&m_pRasterizerState
	);
	assert(hr == S_OK);
	m_pD3DDeviceContext->RSSetState(m_pRasterizerState);

	// set blend state.
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = m_pD3DDevice->CreateBlendState(
		&blendDesc,
		&m_pBlendState
	);
	assert(hr == S_OK);
	f32 blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	m_pD3DDeviceContext->OMSetBlendState(m_pBlendState, blendFactor, 0xffffffff);

	// Now set the viewport.
	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(m_bbDesc.Width);
	viewport.Height = static_cast<float>(m_bbDesc.Height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	// Create the viewport.
	m_pD3DDeviceContext->RSSetViewports(1, &viewport);
}

void RenderMain::__ConfigureBuffers_D2D()
{
	HRESULT hr = S_OK;

	// Direct2D needs the dxgi version of the backbuffer surface pointer.
	IDXGISurface* pDXGISurface = nullptr;
	m_pDXGISwapChain->GetBuffer(0, IID_PPV_ARGS(&pDXGISurface));

	D2D1_BITMAP_PROPERTIES1 bitmapProperties;
	bitmapProperties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	bitmapProperties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
	bitmapProperties.dpiX = static_cast<float>(m_dataRender.m_dpi);
	bitmapProperties.dpiY = static_cast<float>(m_dataRender.m_dpi);
	bitmapProperties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
	bitmapProperties.colorContext = nullptr;

	assert(m_pD2DScreenBitmap == nullptr);
	hr = m_pD2DDeviceContext->CreateBitmapFromDxgiSurface(
		pDXGISurface,
		&bitmapProperties,
		&m_pD2DScreenBitmap
	);
	assert(hr == S_OK);

	// Now we can set the Direct2D render target.
	m_pD2DDeviceContext->SetTarget(m_pD2DScreenBitmap);

	RELEASEI(pDXGISurface);
}

void RenderMain::__ReleaseBuffers()
{
	RELEASEI(m_pBlendState);
	RELEASEI(m_pRasterizerState);
	RELEASEI(m_pDepthStencilView);
	RELEASEI(m_pDepthStencilState);
	RELEASEI(m_pDepthStencilBuffer);
	RELEASEI(m_pRenderTarget);
	RELEASEI(m_pBackBuffer);

	RELEASEI(m_pD2DScreenBitmap);

	// After releasing references to these resources, we need to call Flush() to 
	// ensure that Direct3D also releases any references it might still have to
	// the same resources - such as pipeline bindings.
	m_pD2DDeviceContext->SetTarget(nullptr);
	m_pD3DDeviceContext->Flush();
}

void RenderMain::__ResizeBuffers()
{
	HRESULT hr = S_OK;

	// clear out old buffes.
	__ReleaseBuffers();

	hr = m_pDXGISwapChain->ResizeBuffers(
		2,								// UINT        BufferCount,
		m_dataRender.m_screenSizePixels.x,	// UINT        Width,
		m_dataRender.m_screenSizePixels.y,	// UINT        Height,
		DXGI_FORMAT_UNKNOWN,			// DXGI_FORMAT NewFormat,
		0								// UINT        SwapChainFlags
	);
	assert(hr == S_OK);

	// configure new buffers, based on the new size.
	__ConfigureBuffers();

	// recreate fonts.
	__ClearFonts();
	__CreateFonts();

	// let everyone know we resized the buffers.
	EventMessage* event = EventMessage_RenderAreaResize::Alloc(m_dataRender.m_dpi, m_dataRender.m_scaleSize, m_dataRender.m_screenSizePixels);
	__QueueEvent(&event);
}

float RenderMain::GetAspectRatio()
{
	return static_cast<float>(m_bbDesc.Width) / static_cast<float>(m_bbDesc.Height);
}

s32 RenderMain::GetWidth()
{
	return m_bbDesc.Width;
}

s32 RenderMain::GetHeight()
{
	return m_bbDesc.Height;
}

void RenderMain::BeginUpdate()
{
	// clear the back buffer
	m_pD3DDeviceContext->ClearRenderTargetView(
		m_pRenderTarget,
		m_bgColor
	);

	// clear the depth buffer
	m_pD3DDeviceContext->ClearDepthStencilView(
		m_pDepthStencilView,
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);

	m_pD3DDeviceContext->OMSetRenderTargets(
		1,
		&m_pRenderTarget,
		m_pDepthStencilView
	);
}

void RenderMain::BeginDraw()
{
	m_pD2DDeviceContext->BeginDraw();
}

void RenderMain::EndDraw()
{
	m_pD2DDeviceContext->EndDraw();
}

void RenderMain::EndUpdate()
{
	m_pDXGISwapChain->Present(1, 0);
}

void RenderMain::__EventHandler(EventMessage* message)
{
	switch (message->GetID())
	{
	case EventMessageID_ResizeWindow:
		__EventResizeWindow(static_cast<EventMessage_ResizeWindow*>(message));
		break;
	}
}

void RenderMain::__EventResizeWindow(EventMessage_ResizeWindow* event)
{
	const IVector2 screenSize(event->m_sizeX, event->m_sizeY);
	const s32 dpi = GetDpiForWindow(m_hWnd);

	if ((m_dataRender.m_screenSizePixels == screenSize) && (m_dataRender.m_dpi == dpi))
		return;

	// update the screen size.
	__SetScreenSizePixels(screenSize, dpi);
	m_dataRender.m_isChanged = true;

	__ResizeBuffers();

	// update various transforms.
	__ConfigureViews();
}

}

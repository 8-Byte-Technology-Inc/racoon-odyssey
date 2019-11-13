#include "pch.h"

#include <DirectXMath.h>

#include "common/file_io.h"

#include "RenderTexture.h"
#include "RenderHelper.h"
#include "RenderMain.h"

#include "RenderShaders.h"

namespace TB8
{

struct RenderShaders_VSConstantBufferData_View 
{
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

struct RenderShaders_PSConstantBufferData
{
	DirectX::XMFLOAT4 lightVector;
};

// RenderShader_ConstantBuffer
RenderShader_ConstantBuffer* RenderShader_ConstantBuffer::AllocView(RenderMain* pRenderer)
{
	RenderShader_ConstantBuffer* pObj = TB8_NEW(RenderShader_ConstantBuffer)(pRenderer);
	pObj->__InitializeView(pRenderer);
	return pObj;
}

RenderShader_ConstantBuffer::RenderShader_ConstantBuffer(RenderMain* pRenderer)
	: m_pRenderer(pRenderer)
	, m_pVSConstantBuffer(nullptr)
{
}

RenderShader_ConstantBuffer::~RenderShader_ConstantBuffer()
{
	__Uninitialize();
}

void RenderShader_ConstantBuffer::__InitializeView(RenderMain* pRenderer)
{
	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC constantBufferDesc;
	ZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.ByteWidth = ((sizeof(RenderShaders_VSConstantBufferData_View) + 15) / 16) * 16;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantBufferDesc.MiscFlags = 0;
	constantBufferDesc.StructureByteStride = 0;

	assert(m_pVSConstantBuffer == nullptr);
	hr = m_pRenderer->GetDevice()->CreateBuffer(
		&constantBufferDesc,
		nullptr,
		&m_pVSConstantBuffer
	);
	assert(hr == S_OK);
}

void RenderShader_ConstantBuffer::__Uninitialize()
{
	RELEASEI(m_pVSConstantBuffer);
}

void RenderShader_ConstantBuffer::__Free()
{
	TB8_DEL(this);
}

void RenderShader_ConstantBuffer::SetView(const Matrix4& viewTransform, const Matrix4& projectionTransform)
{
	HRESULT hr = S_OK;
	ID3D11DeviceContext* pContext = m_pRenderer->GetDeviceContext();

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = pContext->Map(m_pVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	assert(hr == S_OK);

	// Get a pointer to the data in the constant buffer.
	RenderShaders_VSConstantBufferData_View* dataPtr = reinterpret_cast<RenderShaders_VSConstantBufferData_View*>(mappedResource.pData);

	// world -> view
	DirectX::XMMATRIX viewMatrix0;
	Matrix4ToXMMATRIX(viewTransform, viewMatrix0);
	DirectX::XMMATRIX viewMatrix1 = DirectX::XMMatrixTranspose(viewMatrix0);
	DirectX::XMStoreFloat4x4(&(dataPtr->view), viewMatrix1);

	// view -> projection
	DirectX::XMMATRIX projectionMatrix0;
	Matrix4ToXMMATRIX(projectionTransform, projectionMatrix0);
	DirectX::XMMATRIX projectionMatrix1 = DirectX::XMMatrixTranspose(projectionMatrix0);
	DirectX::XMStoreFloat4x4(&(dataPtr->projection), projectionMatrix1);

	// Unlock the constant buffer.
	pContext->Unmap(m_pVSConstantBuffer, 0);
}


// RenderShader
RenderShader::RenderShader(RenderMain* pRenderer, RenderShaderID id)
	: ref_count()
	, m_pRenderer(pRenderer)
	, m_id(id)
	, m_pVertexShader(nullptr)
	, m_pInputLayout(nullptr)
	, m_pPixelShader(nullptr)
	, m_pVSConstantBuffer_View(nullptr)
	, m_pVSConstantBuffer_World(nullptr)
	, m_pVSConstantBuffer_Anim(nullptr)
	, m_pVSConstantBuffer_Joints(nullptr)
	, m_pPSConstantBuffer(nullptr)
	, m_pSampleState(nullptr)
	, m_pTexture(nullptr)
	, m_pBoneTexture(nullptr)
{
	m_viewTransform = DirectX::XMMatrixIdentity();
	m_projectionTransform = DirectX::XMMatrixIdentity();
	m_lightVector = DirectX::XMVectorSet(0.f, +1.f, 0.f, 0.f);
}

RenderShader::~RenderShader()
{
	__Shutdown();
}

/* static */ RenderShader* RenderShader::Alloc(RenderMain* pRenderer, RenderShaderID id, const char* path, const char* vertexFileName, const char* pixelFileName)
{
	RenderShader* obj = TB8_NEW(RenderShader)(pRenderer, id);
	obj->__Initialize(path, vertexFileName, pixelFileName);
	return obj;
}

void RenderShader::__Free()
{
	TB8_DEL(this);
}

void RenderShader::SetTexture(RenderTexture* pTexture)
{
	// release previous textures.
	RELEASEI(m_pTexture);
	if (pTexture)
	{
		m_pTexture = pTexture;
		m_pTexture->AddRef();
	}
}

void RenderShader::ApplyRenderState(ID3D11DeviceContext* context)
{
	// update the constant state.
	//ApplyConstantState(context);

	// set the input layout.
	context->IASetInputLayout(m_pInputLayout);

	// Set up the vertex shader stage.
	context->VSSetShader(
		m_pVertexShader,
		nullptr,
		0
	);

	// Set up the pixel shader stage.
	context->PSSetShader(
		m_pPixelShader,
		nullptr,
		0
	);

	// Set shader texture resources in the vertex shader.
	if (m_pBoneTexture)
	{
		ID3D11ShaderResourceView* ppTextures[1] =
		{
			m_pBoneTexture
		};
		context->VSSetShaderResources(
			0,
			ARRAYSIZE(ppTextures),
			ppTextures
		);
	}

	// Set shader texture resources in the pixel shader.
	if (m_pTexture)
	{
		ID3D11ShaderResourceView* ppTextures[1] =
		{
			m_pTexture->GetTexture()
		};
		context->PSSetShaderResources(
			0,
			ARRAYSIZE(ppTextures),
			ppTextures
		);
	}

	// vs constants.
	{
		ID3D11Buffer* ppBuffers[4] = { m_pVSConstantBuffer_View->m_pVSConstantBuffer, m_pVSConstantBuffer_World, m_pVSConstantBuffer_Anim, m_pVSConstantBuffer_Joints };

		context->VSSetConstantBuffers(
			0,
			ARRAYSIZE(ppBuffers),
			ppBuffers
		);
	}

	// ps constants.
	{
		ID3D11Buffer* ppBuffers[1] = { m_pPSConstantBuffer };
		context->PSSetConstantBuffers(
			0,
			ARRAYSIZE(ppBuffers),
			ppBuffers
		);
	}


	// Set the sampler state in the pixel shader.
	ID3D11SamplerState* state[1] = { m_pSampleState };
	context->PSSetSamplers(0, 1, state);
}

void RenderShader::SetViewConstantBuffer(RenderShader_ConstantBuffer* pConstantBuffer)
{
	RELEASEI(m_pVSConstantBuffer_View);
	m_pVSConstantBuffer_View = pConstantBuffer;
	m_pVSConstantBuffer_View->AddRef();
}

void RenderShader::SetLightVector(const Vector3& vector)
{
	DirectX::XMVECTOR lightVector;
	Vector3ToXMVECTOR(vector, lightVector);

	m_lightVector = DirectX::XMVector3Normalize(lightVector);

	__UpdatePSConstants();
}

void RenderShader::SetBoneTexture(ID3D11ShaderResourceView* pBoneTexture)
{
	RELEASEI(m_pBoneTexture);
	m_pBoneTexture = pBoneTexture;
	m_pBoneTexture->AddRef();
}

void RenderShader::SetModelVSConstants_World(ID3D11Buffer* pVSConstants)
{
	RELEASEI(m_pVSConstantBuffer_World);
	m_pVSConstantBuffer_World = pVSConstants;
	m_pVSConstantBuffer_World->AddRef();
}

void RenderShader::SetModelVSConstants_Anim(ID3D11Buffer* pVSConstants)
{
	RELEASEI(m_pVSConstantBuffer_Anim);
	m_pVSConstantBuffer_Anim = pVSConstants;
	m_pVSConstantBuffer_Anim->AddRef();
}

void RenderShader::SetModelVSConstants_Joints(ID3D11Buffer* pVSConstants)
{
	RELEASEI(m_pVSConstantBuffer_Joints);
	m_pVSConstantBuffer_Joints = pVSConstants;
	m_pVSConstantBuffer_Joints->AddRef();
}

void RenderShader::__Initialize(const char* path, const char* vertexFileName, const char* pixelFileName)
{
	ID3D11Device* device = m_pRenderer->GetDevice();
	HRESULT hr = S_OK;

	// vertex shader
	std::string vertexFilePath = path;
	TB8::File::AppendToPath(vertexFilePath, vertexFileName);

	std::vector<u8> vertexShaderBuffer;
	hr = LoadFile(vertexFilePath.c_str(), vertexShaderBuffer);
	assert(hr == S_OK);

	assert(m_pVertexShader == nullptr);
	hr = device->CreateVertexShader(
		vertexShaderBuffer.data(),
		vertexShaderBuffer.size(),
		nullptr,
		&m_pVertexShader
	);
	assert(hr == S_OK);

	// pixel shader
	std::string pixelFilePath = path;
	TB8::File::AppendToPath(pixelFilePath, pixelFileName);

	std::vector<u8> pixelShaderBuffer;
	hr = LoadFile(pixelFilePath.c_str(), pixelShaderBuffer);
	assert(hr == S_OK);

	assert(m_pPixelShader == nullptr);
	hr = device->CreatePixelShader(
		pixelShaderBuffer.data(),
		pixelShaderBuffer.size(),
		nullptr,
		&m_pPixelShader
	);
	assert(hr == S_OK);

	// input layout
	D3D11_INPUT_ELEMENT_DESC iaDesc[5] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
		0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,
		0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
		0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		{ "BONES", 0, DXGI_FORMAT_R32G32B32A32_SINT,
		0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
		0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	assert(m_pInputLayout == nullptr);
	hr = device->CreateInputLayout(
		iaDesc,
		ARRAYSIZE(iaDesc),
		vertexShaderBuffer.data(),
		vertexShaderBuffer.size(),
		&m_pInputLayout
	);
	assert(hr == S_OK);

	// ps constant buffers.
	{
		D3D11_BUFFER_DESC constantBufferDesc;
		ZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.ByteWidth = ((sizeof(RenderShaders_PSConstantBufferData) + 15) / 16) * 16;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;
		constantBufferDesc.StructureByteStride = 0;

		assert(m_pPSConstantBuffer == nullptr);
		hr = device->CreateBuffer(
			&constantBufferDesc,
			nullptr,
			&m_pPSConstantBuffer
		);
		assert(hr == S_OK);
	}

	// sampler state.
	CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	device->CreateSamplerState(
		&samplerDesc,
		&m_pSampleState);
}

void RenderShader::__UpdatePSConstants()
{
	HRESULT hr = S_OK;
	ID3D11DeviceContext* pContext = m_pRenderer->GetDeviceContext();

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = pContext->Map(m_pPSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	assert(hr == S_OK);

	// Get a pointer to the data in the constant buffer.
	RenderShaders_PSConstantBufferData* dataPtr = reinterpret_cast<RenderShaders_PSConstantBufferData*>(mappedResource.pData);

	// light vector
	DirectX::XMStoreFloat4(&(dataPtr->lightVector), m_lightVector);

	// Unlock the constant buffer.
	pContext->Unmap(m_pPSConstantBuffer, 0);
}

void RenderShader::__Shutdown()
{
	RELEASEI(m_pBoneTexture);
	RELEASEI(m_pTexture);
	RELEASEI(m_pVertexShader);
	RELEASEI(m_pInputLayout);
	RELEASEI(m_pPixelShader);
	RELEASEI(m_pPSConstantBuffer);
	RELEASEI(m_pVSConstantBuffer_Joints);
	RELEASEI(m_pVSConstantBuffer_Anim);
	RELEASEI(m_pVSConstantBuffer_World);
	RELEASEI(m_pVSConstantBuffer_View);
	RELEASEI(m_pSampleState);
}

}

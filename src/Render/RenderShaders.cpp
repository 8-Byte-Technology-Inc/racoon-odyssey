#include "pch.h"

#include <DirectXMath.h>

#include "common/file_io.h"

#include "RenderTexture.h"
#include "RenderHelper.h"
#include "RenderMain.h"

#include "RenderShaders.h"

namespace TB8
{

struct RenderShaders_VSConstantBufferData 
{
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

struct RenderShaders_PSConstantBufferData
{
	DirectX::XMFLOAT4 lightVector;
};

RenderShader::RenderShader(RenderMain* pRenderer, RenderShaderID id)
	: ref_count()
	, m_pRenderer(pRenderer)
	, m_id(id)
	, m_pVertexShader(nullptr)
	, m_pInputLayout(nullptr)
	, m_pPixelShader(nullptr)
	, m_pVSConstantBuffer(nullptr)
	, m_pPSConstantBuffer(nullptr)
	, m_pVSConstantBufferModel(nullptr)
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
		ID3D11Buffer* ppBuffers[2] = { m_pVSConstantBuffer, m_pVSConstantBufferModel };

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

void RenderShader::SetViewTransform(const Matrix4& view)
{
	Matrix4ToXMMATRIX(view, m_viewTransform);
	__UpdateVSConstants();
}

void RenderShader::SetProjectionTransform(const Matrix4& projection)
{
	Matrix4ToXMMATRIX(projection, m_projectionTransform);
	__UpdateVSConstants();
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

void RenderShader::SetModelVSConstants(ID3D11Buffer* pVSConstants)
{
	RELEASEI(m_pVSConstantBufferModel);
	m_pVSConstantBufferModel = pVSConstants;
	m_pVSConstantBufferModel->AddRef();
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

	// vs constant buffers.
	{
		D3D11_BUFFER_DESC constantBufferDesc;
		ZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.ByteWidth = ((sizeof(RenderShaders_VSConstantBufferData) + 15) / 16) * 16;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;
		constantBufferDesc.StructureByteStride = 0;

		assert(m_pVSConstantBuffer == nullptr);
		hr = device->CreateBuffer(
			&constantBufferDesc,
			nullptr,
			&m_pVSConstantBuffer
		);
		assert(hr == S_OK);
	}

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

void RenderShader::__UpdateVSConstants()
{
	HRESULT hr = S_OK;
	ID3D11DeviceContext* pContext = m_pRenderer->GetDeviceContext();

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = pContext->Map(m_pVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	assert(hr == S_OK);

	// Get a pointer to the data in the constant buffer.
	RenderShaders_VSConstantBufferData* dataPtr = reinterpret_cast<RenderShaders_VSConstantBufferData*>(mappedResource.pData);

	// world -> view
	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixTranspose(m_viewTransform);
	DirectX::XMStoreFloat4x4(&(dataPtr->view), viewMatrix);

	// view -> projection
	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixTranspose(m_projectionTransform);
	DirectX::XMStoreFloat4x4(&(dataPtr->projection), projectionMatrix);

	// Unlock the constant buffer.
	pContext->Unmap(m_pVSConstantBuffer, 0);
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
	RELEASEI(m_pVSConstantBufferModel);
	RELEASEI(m_pPSConstantBuffer);
	RELEASEI(m_pVSConstantBuffer);
	RELEASEI(m_pSampleState);
}

}

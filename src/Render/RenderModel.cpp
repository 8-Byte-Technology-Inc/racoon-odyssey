#include "pch.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "RenderHelper.h"
#include "RenderModel.h"
#include "RenderTexture.h"
#include "RenderShaders.h"
#include "RenderMain.h"

namespace TB8
{

RenderModel::RenderModel(RenderMain* pRenderer)
	: ref_count()
	, m_pRenderer(pRenderer)
	, m_vertexCount(0)
	, m_pVertexBuffer(nullptr)
	, m_indexCount(0)
	, m_pIndexBuffer(nullptr)
	, m_pTexture(nullptr)
	, m_pShader(nullptr)
	, m_position(Vector3(0.f, 0.f, 0.f))
	, m_scale(Vector3(1.f, 1.f, 1.f))
	, m_rotation(Vector3(0.f, 0.f, 0.f))
	, m_animIndex(-1)
	, m_pBoneTexture(nullptr)
	, m_pVSConstantBuffer(nullptr)
{
	m_coordTranslate.SetIdentity();
	m_worldTransform.SetIdentity();
}

RenderModel::~RenderModel()
{
	RELEASEI(m_pVSConstantBuffer);
	RELEASEI(m_pBoneTexture);
	RELEASEI(m_pShader);
	RELEASEI(m_pTexture);
	RELEASEI(m_pIndexBuffer);
	RELEASEI(m_pVertexBuffer);
}

/* static */ RenderModel* RenderModel::Alloc(RenderMain* pRenderer, s32 vertexCount, RenderModel_VertexPositionTexture* verticies, s32 indexCount, u16* indicies, const Vector4& color)
{
	RenderModel* obj = TB8_NEW(RenderModel)(pRenderer);
	obj->__Initialize(pRenderer, vertexCount, verticies, indexCount, indicies, color);
	return obj;
}

/* static */ RenderModel* RenderModel::AllocFromDAE(RenderMain* pRenderer, const char* path, const char* file, const char* modelName)
{
	RenderModel* obj = TB8_NEW(RenderModel)(pRenderer);
	obj->__InitializeFromDAE(pRenderer, path, file, modelName);
	return obj;
}

void RenderModel::Render(RenderMain* pRenderer)
{
	ID3D11DeviceContext* pContext = pRenderer->GetDeviceContext();

	// set vertex buffer.
	ID3D11Buffer* vertexBuffers[1] = { m_pVertexBuffer };
	UINT stride = sizeof(RenderShader_Vertex_Generic);
	UINT offset = 0;
	pContext->IASetVertexBuffers(
		0,
		ARRAYSIZE(vertexBuffers),
		vertexBuffers,
		&stride,
		&offset
	);

	// set index buffer.
	pContext->IASetIndexBuffer(
		m_pIndexBuffer,
		DXGI_FORMAT_R16_UINT,
		0
	);

	// set up type of primitives we're using.
	pContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);

	// set bones.
	if (m_pBoneTexture)
	{
		m_pShader->SetBoneTexture(m_pBoneTexture);
	}

	// set model constants, which includes the world transform & animation index.
	m_pShader->SetModelVSConstants(m_pVSConstantBuffer);

	// set the world transform.
	//m_pShader->SetWorldTransform(m_worldTransform);

	// set the texture for the shader to use.
	m_pShader->SetTexture(m_pTexture);

	// set the shader to use.
	m_pShader->ApplyRenderState(pContext);

	// draw !!
	pContext->DrawIndexed(
		m_indexCount,
		0,
		0
	);
}

void RenderModel::SetPosition(const Vector3& position) 
{ 
	if (m_position == position)
		return;

	m_position = position;
	__UpdateVSConstants(); 
}

void RenderModel::SetRotation(const Vector3& rotation) 
{ 
	if (m_rotation == rotation)
		return;

	m_rotation = rotation;
	__UpdateVSConstants(); 
}

void RenderModel::SetScale(const Vector3& scale) 
{ 
	if (m_scale == scale)
		return;

	m_scale = scale;
	__UpdateVSConstants(); 
}

void RenderModel::SetAnimID(const s32 animID)
{
	if (m_anims.empty())
		return;

	m_animIndex = 0;

	s32 index = 0;
	for (std::vector<RenderModel_Anims>::iterator it = m_anims.begin(); it != m_anims.end(); ++it, ++index)
	{
		if (it->m_animID <= animID)
		{
			m_animIndex = index;
		}
	}

	__UpdateVSConstants();
}

void RenderModel::GetNamedVerticies(std::vector<RenderModel_NamedVertex>* pVerticies) const
{
	pVerticies->reserve(m_namedVerticies.size());
	for (std::vector<RenderModel_NamedVertex>::const_iterator it = m_namedVerticies.begin(); it != m_namedVerticies.end(); ++it)
	{
		const RenderModel_NamedVertex& src = *it;
		RenderModel_NamedVertex dst;
		dst.m_name = src.m_name;
		dst.m_vertex = Matrix4::MultiplyVector(src.m_vertex, m_coordTranslate);
		pVerticies->push_back(dst);
	}
}

void RenderModel::__InitVSConstantBuffer()
{
	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC constantBufferDesc;
	ZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.ByteWidth = ((sizeof(RenderShaders_Model_VSConstantants) + 15) / 16) * 16;
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

	__UpdateVSConstants();
}

void RenderModel::__UpdateVSConstants()
{
	HRESULT hr = S_OK;

	Matrix4 userScale;
	Matrix4 userRotation;
	Matrix4 userPosition;

	userScale.SetScale(m_scale);
	userRotation.SetRotate(m_rotation);
	userPosition.SetTranslation(m_position);

	m_worldTransform = Matrix4::MultiplyAB(userScale, userRotation);
	m_worldTransform = Matrix4::MultiplyAB(userPosition, m_worldTransform);

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = m_pRenderer->GetDeviceContext()->Map(m_pVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	assert(hr == S_OK);

	// Get a pointer to the data in the constant buffer.
	RenderShaders_Model_VSConstantants* dataPtr = reinterpret_cast<RenderShaders_Model_VSConstantants*>(mappedResource.pData);

	// model -> world (verticies)
	{
		const Matrix4 worldMatrixA = Matrix4::MultiplyAB(m_worldTransform, m_coordTranslate);
		DirectX::XMMATRIX worldMatrix1;
		Matrix4ToXMMATRIX(worldMatrixA, worldMatrix1);
		DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixTranspose(worldMatrix1);
		DirectX::XMStoreFloat4x4(&(dataPtr->world), worldMatrix);
	}

	// model -> world (normals)
	{
		const Matrix4 worldNormalMatrixA = Matrix4::MultiplyAB(m_worldTransform, m_coordTranslate);
		const Matrix4 worldNormalMatrixB = Matrix4::ExtractRotation(worldNormalMatrixA);

		DirectX::XMMATRIX worldNormalMatrix1;
		Matrix4ToXMMATRIX(worldNormalMatrixB, worldNormalMatrix1);
		DirectX::XMMATRIX worldNormalMatrix2 = DirectX::XMMatrixInverse(nullptr, worldNormalMatrix1);
		//DirectX::XMMATRIX worldInverseTranspose = DirectX::XMMatrixTranspose(worldInverse);

		DirectX::XMStoreFloat4x4(&(dataPtr->worldNormalMatrix), worldNormalMatrix2);
	}

	// anim index.
	dataPtr->animIndex = m_animIndex;

	// Unlock the constant buffer.
	m_pRenderer->GetDeviceContext()->Unmap(m_pVSConstantBuffer, 0);
}

void RenderModel::__Initialize(RenderMain* pRenderer, s32 vertexCount, RenderModel_VertexPositionTexture* verticiesIn, s32 indexCount, u16* indicies, const Vector4& color)
{
	HRESULT hr = S_OK;
	ID3D11Device* device = pRenderer->GetDevice();

	assert(!"not yet implemented !");
}

void RenderModel::__Free()
{
	TB8_DEL(this);
}

RenderModel_NamedVertex* RenderModel::__FindNamedVertex(const char* name)
{
	for (std::vector<RenderModel_NamedVertex>::iterator it = m_namedVerticies.begin(); it != m_namedVerticies.end(); ++it)
	{
		RenderModel_NamedVertex& node = *it;
		if (node.m_name == name)
		{
			return &node;
		}
	}
	return nullptr;
}

}
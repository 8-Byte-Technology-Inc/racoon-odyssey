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

void RenderModel_Bounds::AddVector(const Vector3& v)
{
	m_min.x = std::min<f32>(m_min.x, v.x);
	m_max.x = std::max<f32>(m_max.x, v.x);

	m_min.y = std::min<f32>(m_min.y, v.y);
	m_max.y = std::max<f32>(m_max.y, v.y);

	m_min.z = std::min<f32>(m_min.z, v.z);
	m_max.z = std::max<f32>(m_max.z, v.z);
}

void RenderModel_Bounds::ComputeCenterAndSize()
{
	m_center = Vector3((m_max.x + m_min.x) / 2.f, (m_max.y + m_min.y) / 2.f, (m_max.z + m_min.z) / 2.f);
	m_size = Vector3(m_max.x - m_min.x, m_max.y - m_min.y, m_max.z - m_min.z);
}

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
	, m_cJoints(0)
	, m_isJointsDirty(false)
	, m_pBoneTexture(nullptr)
	, m_pVSConstantBuffer_World(nullptr)
	, m_pVSConstantBuffer_Anim(nullptr)
	, m_pVSConstantBuffer_Joints(nullptr)
{
	m_coordTranslate.SetIdentity();
	m_worldTransform.SetIdentity();
}

RenderModel::~RenderModel()
{
	RELEASEI(m_pVSConstantBuffer_Joints);
	RELEASEI(m_pVSConstantBuffer_Anim);
	RELEASEI(m_pVSConstantBuffer_World);
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

/* static */ RenderModel* RenderModel::AllocSquareFromTexture(RenderMain* pRenderer, const Vector3& v0, const Vector3& v1, const char* pszTexturePath)
{
	RenderModel* obj = TB8_NEW(RenderModel)(pRenderer);
	obj->__InitializeSquareFromTexture(pRenderer, v0, v1, pszTexturePath);
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

	// update joints, if dirty.
	if (m_isJointsDirty)
	{
		__UpdateVSConstants_Joints();
	}

	// set model constants, which includes the world transform & animation index.
	m_pShader->SetModelVSConstants_World(m_pVSConstantBuffer_World);
	m_pShader->SetModelVSConstants_Anim(m_pVSConstantBuffer_Anim);
	m_pShader->SetModelVSConstants_Joints(m_pVSConstantBuffer_Joints);

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

	Matrix4 userScale;
	Matrix4 userRotation;
	Matrix4 userPosition;

	userScale.SetScale(m_scale);
	userRotation.SetRotate(m_rotation);
	userPosition.SetTranslation(m_position);

	m_worldTransform = Matrix4::MultiplyAB(userScale, userRotation);
	m_worldTransform = Matrix4::MultiplyAB(userPosition, m_worldTransform);

	__UpdateVSConstants_World();
}

void RenderModel::SetRotation(const Vector3& rotation) 
{ 
	if (m_rotation == rotation)
		return;

	m_rotation = rotation;

	Matrix4 userScale;
	Matrix4 userRotation;
	Matrix4 userPosition;

	userScale.SetScale(m_scale);
	userRotation.SetRotate(m_rotation);
	userPosition.SetTranslation(m_position);

	m_worldTransform = Matrix4::MultiplyAB(userScale, userRotation);
	m_worldTransform = Matrix4::MultiplyAB(userPosition, m_worldTransform);

	__UpdateVSConstants_World();
}

void RenderModel::SetScale(const Vector3& scale) 
{ 
	if (m_scale == scale)
		return;

	m_scale = scale;

	Matrix4 userScale;
	Matrix4 userRotation;
	Matrix4 userPosition;

	userScale.SetScale(m_scale);
	userRotation.SetRotate(m_rotation);
	userPosition.SetTranslation(m_position);

	m_worldTransform = Matrix4::MultiplyAB(userScale, userRotation);
	m_worldTransform = Matrix4::MultiplyAB(userPosition, m_worldTransform);

	__UpdateVSConstants_World();
}

void RenderModel::SetWorldTransform(const Matrix4& transform)
{
	if (m_worldTransform == transform)
		return;

	m_worldTransform = transform;
	__UpdateVSConstants_World();
}

u32 RenderModel::GetAnimCount() const
{
	return !m_anims.empty() ? static_cast<u32>(m_anims.size() - 1) : 0;
}

const RenderModel_Anim* RenderModel::GetAnim(u32 animID) const
{
	for (std::vector<RenderModel_Anim>::const_iterator it = m_anims.begin(); it != m_anims.end(); ++it)
	{
		if (it->m_animID == animID)
			return &(*it);
	}
	return nullptr;
}

void RenderModel::SetAnimID(const s32 animID)
{
	if (m_anims.empty())
		return;

	m_animIndex = 0;

	s32 index = 0;
	for (std::vector<RenderModel_Anim>::iterator it = m_anims.begin(); it != m_anims.end(); ++it, ++index)
	{
		if (it->m_animID <= animID)
		{
			m_animIndex = index;
		}
	}

	__UpdateVSConstants_Anim();
}

void RenderModel::SetJointRotation(u32 jointIndex, const Vector3& rotation)
{
	assert(jointIndex < m_cJoints);
	if (jointIndex == 0)
		return;

	RenderModel_Joint& joint = m_joints[jointIndex];

	Matrix4 rotateMatrix;
	rotateMatrix.SetRotate(rotation);

	joint.m_effectiveMatrix = Matrix4::MultiplyAB(joint.m_baseMatrix, rotateMatrix);
	joint.m_isDirty = true;
	m_isJointsDirty = true;
}

void RenderModel::ResetJointTransformMatricies()
{
	// reset to base transforms.
	for (u32 i = 0; i < m_cJoints; ++i)
	{
		RenderModel_Joint& joint = m_joints[i];
		joint.m_effectiveMatrix = joint.m_baseMatrix;
		joint.m_isDirty = true;
	}
	m_isJointsDirty = true;
}

void RenderModel::SetJointTransformMatrix(u32 jointIndex, const Matrix4& transform)
{
	assert(jointIndex < m_cJoints);
	if (jointIndex == 0)
		return;

	RenderModel_Joint& joint = m_joints[jointIndex];

	joint.m_effectiveMatrix = transform;
	joint.m_isDirty = true;
	m_isJointsDirty = true;
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

void RenderModel::__InitVSConstantBuffers()
{
	HRESULT hr = S_OK;

	{
		D3D11_BUFFER_DESC constantBufferDesc;
		ZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.ByteWidth = ((sizeof(RenderShaders_Model_VSConstantants_World) + 15) / 16) * 16;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;
		constantBufferDesc.StructureByteStride = 0;

		assert(m_pVSConstantBuffer_World == nullptr);
		hr = m_pRenderer->GetDevice()->CreateBuffer(
			&constantBufferDesc,
			nullptr,
			&m_pVSConstantBuffer_World
		);
		assert(hr == S_OK);
	}

	{
		D3D11_BUFFER_DESC constantBufferDesc;
		ZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.ByteWidth = ((sizeof(RenderShaders_Model_VSConstantants_Anim) + 15) / 16) * 16;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;
		constantBufferDesc.StructureByteStride = 0;

		assert(m_pVSConstantBuffer_Anim == nullptr);
		hr = m_pRenderer->GetDevice()->CreateBuffer(
			&constantBufferDesc,
			nullptr,
			&m_pVSConstantBuffer_Anim
		);
		assert(hr == S_OK);
	}

	{
		D3D11_BUFFER_DESC constantBufferDesc;
		ZeroMemory(&constantBufferDesc, sizeof(constantBufferDesc));
		constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantBufferDesc.ByteWidth = ((sizeof(RenderShaders_Model_VSConstantants_Joints) + 15) / 16) * 16;
		constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantBufferDesc.MiscFlags = 0;
		constantBufferDesc.StructureByteStride = 0;

		assert(m_pVSConstantBuffer_Joints == nullptr);
		hr = m_pRenderer->GetDevice()->CreateBuffer(
			&constantBufferDesc,
			nullptr,
			&m_pVSConstantBuffer_Joints
		);
		assert(hr == S_OK);
	}

	__UpdateVSConstants_World();
	__UpdateVSConstants_Anim();
	__UpdateVSConstants_Joints();
}

void RenderModel::__UpdateVSConstants_World()
{
	HRESULT hr = S_OK;

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = m_pRenderer->GetDeviceContext()->Map(m_pVSConstantBuffer_World, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	assert(hr == S_OK);

	// Get a pointer to the data in the constant buffer.
	RenderShaders_Model_VSConstantants_World* dataPtr = reinterpret_cast<RenderShaders_Model_VSConstantants_World*>(mappedResource.pData);

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

	// Unlock the constant buffer.
	m_pRenderer->GetDeviceContext()->Unmap(m_pVSConstantBuffer_World, 0);
}

void RenderModel::__UpdateVSConstants_Anim()
{
	HRESULT hr = S_OK;

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = m_pRenderer->GetDeviceContext()->Map(m_pVSConstantBuffer_Anim, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	assert(hr == S_OK);

	// Get a pointer to the data in the constant buffer.
	RenderShaders_Model_VSConstantants_Anim* dataPtr = reinterpret_cast<RenderShaders_Model_VSConstantants_Anim*>(mappedResource.pData);

	// anim index.
	dataPtr->animIndex = m_animIndex;

	// Unlock the constant buffer.
	m_pRenderer->GetDeviceContext()->Unmap(m_pVSConstantBuffer_Anim, 0);
}

void RenderModel::__UpdateJointMatricies()
{
	// update computed matricies for dirty joints and their children.
	for (u32 i = 0; i < m_cJoints; ++i)
	{
		// if there is a parent, start with that.
		RenderModel_Joint& src = m_joints[i];
		const bool isNeedsRecompute = src.m_isDirty || ((src.m_parentIndex >= 0) && m_joints[src.m_parentIndex].m_isDirty);
		if (!isNeedsRecompute)
			continue;

		if (src.m_parentIndex >= 0)
		{
			const Matrix4& matrixParent = m_joints[src.m_parentIndex].m_computedMatrix;
			src.m_computedMatrix = Matrix4::MultiplyAB(matrixParent, src.m_effectiveMatrix);
		}
		else
		{
			src.m_computedMatrix = Matrix4::MultiplyAB(m_baseJointMatrix, src.m_effectiveMatrix);
		}

		src.m_boundMatrix = Matrix4::MultiplyAB(src.m_computedMatrix, src.m_baseInvBindMatrix);
		src.m_isDirty = true;
	}

	// mark everything as not dirty.
	for (u32 i = 0; i < m_cJoints; ++i)
	{
		RenderModel_Joint& src = m_joints[i];
		src.m_isDirty = false;
	}
}

void RenderModel::__UpdateVSConstants_Joints()
{
	HRESULT hr = S_OK;

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = m_pRenderer->GetDeviceContext()->Map(m_pVSConstantBuffer_Joints, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	assert(hr == S_OK);

	// Get a pointer to the data in the constant buffer.
	RenderShaders_Model_VSConstantants_Joints* dataPtr = reinterpret_cast<RenderShaders_Model_VSConstantants_Joints*>(mappedResource.pData);

	// update computed matricies for dirty joints and their children.
	__UpdateJointMatricies();

	// apply matricies to constants.
	for (u32 i = 0; i < m_cJoints; ++i)
	{
		RenderModel_Joint& src = m_joints[i];

		// compute matrix to apply to verticies.
		{
			DirectX::XMMATRIX worldMatrix1;
			Matrix4ToXMMATRIX(src.m_boundMatrix, worldMatrix1);
			DirectX::XMMATRIX worldMatrix2 = DirectX::XMMatrixTranspose(worldMatrix1);
			DirectX::XMStoreFloat4x4(&(dataPtr->jointMatrix[i]), worldMatrix2);
		}

		// compute matrix to apply to vertex normals.
		{
			const Matrix4 matrixNormal1 = Matrix4::ExtractRotation(src.m_boundMatrix);
			DirectX::XMMATRIX matrixNormal2;
			Matrix4ToXMMATRIX(matrixNormal1, matrixNormal2);
			DirectX::XMMATRIX matrixNormal3 = DirectX::XMMatrixInverse(nullptr, matrixNormal2);
			DirectX::XMMATRIX matrixNormal4 = DirectX::XMMatrixTranspose(matrixNormal3);
			DirectX::XMStoreFloat4x4(&(dataPtr->jointNormalMatrix[i]), matrixNormal4);
		}
	}

	// Unlock the constant buffer.
	m_pRenderer->GetDeviceContext()->Unmap(m_pVSConstantBuffer_Joints, 0);

	// no longer dirty.
	m_isJointsDirty = false;
}

void RenderModel::__Initialize(RenderMain* pRenderer, s32 vertexCount, RenderModel_VertexPositionTexture* verticiesIn, s32 indexCount, u16* indicies, const Vector4& color)
{
	HRESULT hr = S_OK;
	ID3D11Device* device = pRenderer->GetDevice();

	assert(!"not yet implemented !");
}

void RenderModel::__InitializeSquareFromTexture(RenderMain* pRenderer, const Vector3& v0, const Vector3& v1, const char* pszTexturePath)
{
	HRESULT hr = S_OK;

	// get a reference to the shader.
	m_pShader = pRenderer->GetShaderByID(RenderShaderID_Generic);
	m_pShader->AddRef();

	// load texture.
	m_pTexture = RenderTexture::Alloc(pRenderer, pszTexturePath);

	const Vector3 vo(0.f, 0.f, 0.f);

	// compute normal.
	const Vector3 normal = Vector3::ComputeNormal(vo, v0, v1);

	// construct verticies & indicies.
	std::vector<RenderShader_Vertex_Generic> verticies;
	std::vector<u16> indicies;
	verticies.reserve(4);
	indicies.reserve(6);

	// v0
	RenderShader_Vertex_Generic vertex;
	Vector2 targetTexPos;
	m_pTexture->MapUV(0, Vector2(0.f, 0.f), &targetTexPos);
	vertex.position.x = vo.x;
	vertex.position.y = vo.y;
	vertex.position.z = vo.z;
	vertex.normal.x = normal.x;
	vertex.normal.y = normal.y;
	vertex.normal.z = normal.z;
	vertex.color.x = targetTexPos.x;
	vertex.color.y = targetTexPos.y;
	vertex.color.z = 0.f;
	vertex.color.w = 0.f;
	vertex.bones.x = -1;
	verticies.push_back(vertex);

	// v1
	m_pTexture->MapUV(0, Vector2(1.f, 0.f), &targetTexPos);
	vertex.position.x = v0.x;
	vertex.position.y = v0.y;
	vertex.position.z = v0.z;
	vertex.color.x = targetTexPos.x;
	vertex.color.y = targetTexPos.y;
	verticies.push_back(vertex);

	// v2
	m_pTexture->MapUV(0, Vector2(1.f, 1.f), &targetTexPos);
	vertex.position.x = v0.x + v1.x;
	vertex.position.y = v0.y + v1.y;
	vertex.position.z = v0.z + v1.z;
	vertex.color.x = targetTexPos.x;
	vertex.color.y = targetTexPos.y;
	verticies.push_back(vertex);

	// v3
	m_pTexture->MapUV(0, Vector2(0.f, 1.f), &targetTexPos);
	vertex.position.x = v1.x;
	vertex.position.y = v1.y;
	vertex.position.z = v1.z;
	vertex.color.x = targetTexPos.x;
	vertex.color.y = targetTexPos.y;
	verticies.push_back(vertex);

	indicies.push_back(0);
	indicies.push_back(1);
	indicies.push_back(2);

	indicies.push_back(0);
	indicies.push_back(2);
	indicies.push_back(3);

	// build mesh desc.
	m_meshes.emplace_back();
	RenderModel_Mesh& mesh = m_meshes.back();
	mesh.m_bounds.AddVector(vo);
	mesh.m_bounds.AddVector(v0);
	mesh.m_bounds.AddVector(v0 + v1);
	mesh.m_bounds.AddVector(v1);
	mesh.m_bounds.ComputeCenterAndSize();

	// build verticies.
	m_vertexCount = static_cast<s32>(verticies.size());

	// construct the vertex buffer.
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(RenderShader_Vertex_Generic) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = sizeof(RenderShader_Vertex_Generic);

	D3D11_SUBRESOURCE_DATA vertexData;
	ZeroMemory(&vertexData, sizeof(D3D11_SUBRESOURCE_DATA));
	vertexData.pSysMem = verticies.data();
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	assert(m_pVertexBuffer == nullptr);
	hr = pRenderer->GetDevice()->CreateBuffer(
		&vertexBufferDesc,
		&vertexData,
		&m_pVertexBuffer
	);
	assert(hr == S_OK);

	// build indicies
	assert(indicies.size() < 0x10000);
	m_indexCount = static_cast<s32>(indicies.size());

	// construct the index buffer.
	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(u16) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = sizeof(u16);

	D3D11_SUBRESOURCE_DATA indexData;
	ZeroMemory(&indexData, sizeof(D3D11_SUBRESOURCE_DATA));
	indexData.pSysMem = indicies.data();
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	assert(m_pIndexBuffer == nullptr);
	hr = pRenderer->GetDevice()->CreateBuffer(
		&indexBufferDesc,
		&indexData,
		&m_pIndexBuffer
	);
	assert(hr == S_OK);

	// init constant buffer.
	__InitVSConstantBuffers();
}

void RenderModel::__Free()
{
	TB8_DEL(this);
}

const RenderModel_Anim_Joint* RenderModel::__GetAnimJoint(s32 animID, s32 jointIndex)
{
	for (std::vector<RenderModel_Anim>::iterator itAnim = m_anims.begin(); itAnim != m_anims.end(); ++itAnim)
	{
		const RenderModel_Anim& anim = *itAnim;
		if (anim.m_animID < animID)
			continue;
		if (anim.m_animID != animID)
			break;

		for (std::vector<RenderModel_Anim_Joint>::const_iterator itJoint = anim.m_joints.begin(); itJoint != anim.m_joints.end(); ++itJoint)
		{
			const RenderModel_Anim_Joint& joint = *itJoint;
			if (joint.m_pJoint->m_index != jointIndex)
				continue;
			return &joint;
		}
	}
	return nullptr;
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
#include "pch.h"

#include "common/memory.h"

#include "render/RenderModel.h"
#include "render/RenderMain.h"

#include "Bounds.h"
#include "Object.h"

namespace TB8
{

World_Object* World_Object::Alloc(Client_Globals* pGlobalState)
{
	World_Object* pObj = TB8_NEW(World_Object)(pGlobalState);
	return pObj;
}

void World_Object::Free()
{
	TB8_DEL(this);
}

void World_Object::Init()
{
	__ComputeModelWorldLocalTransform();
	ComputeBounds();
}

void World_Object::Render(const Vector3& screenWorldPos)
{
	// rotate it.
	Matrix4 matrixRotate;
	matrixRotate.SetRotate(Vector3(0.f, 0.f, DirectX::XM_PI * (m_rotation) / 180.f));

	// position it.
	Vector3 renderPos = m_pos - screenWorldPos;

	Matrix4 matrixPosition;
	matrixPosition.SetTranslation(renderPos);

	// compute the overall transform.
	Matrix4 worldTransform = matrixPosition;
	worldTransform = Matrix4::MultiplyAB(worldTransform, matrixRotate);
	worldTransform = Matrix4::MultiplyAB(worldTransform, m_worldLocalTransform);

	m_pModel->SetWorldTransform(worldTransform);
	m_pModel->Render(__GetRenderer());
}

void World_Object::__ComputeModelWorldLocalTransform()
{
	const std::vector<RenderModel_Mesh>& meshes = m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	// center it, still in directX coords.
	Matrix4 matrixCenter;
	matrixCenter.SetTranslation(Vector3(0.f - ((mesh.m_max.x + mesh.m_min.x) / 2.f), 0.f - mesh.m_min.y, 0.f - ((mesh.m_max.z + mesh.m_min.z) / 2.f)));

	// align size of the model.
	const Vector3 sizeBase(mesh.m_max.x - mesh.m_min.x, mesh.m_max.y - mesh.m_min.y, mesh.m_max.z - mesh.m_min.z);
	Vector3 sizeAligned = sizeBase * m_scale;
	__GetRenderer()->AlignWorldSize(sizeAligned);

	// scale it.
	const Vector3 scale(is_approx_zero(sizeBase.x) ? 1.f : (sizeAligned.x / sizeBase.x),
		is_approx_zero(sizeBase.y) ? 1.f : (sizeAligned.y / sizeBase.y),
		is_approx_zero(sizeBase.z) ? 1.f : (sizeAligned.z / sizeBase.z));

	Matrix4 matrixScale;
	matrixScale.SetScale(scale);

	// translate from directX to world coordinate system.
	Matrix4 matrixCoordsToWorld;
	matrixCoordsToWorld.SetIdentity();
	matrixCoordsToWorld.m[1][1] = 0.0f;
	matrixCoordsToWorld.m[2][1] = -1.0f;
	matrixCoordsToWorld.m[2][2] = 0.0f;
	matrixCoordsToWorld.m[1][2] = 1.0f;

	// offset.
	Matrix4 matrixOffset;
	matrixOffset.SetTranslation(m_offset);

	m_worldLocalTransform = matrixOffset;
	m_worldLocalTransform = Matrix4::MultiplyAB(m_worldLocalTransform, matrixCoordsToWorld);
	m_worldLocalTransform = Matrix4::MultiplyAB(m_worldLocalTransform, matrixScale);
	m_worldLocalTransform = Matrix4::MultiplyAB(m_worldLocalTransform, matrixCenter);
}

void World_Object::ComputeBounds()
{
	m_bounds.ComputeBounds(*this);
}

bool World_Object::IsCollision(World_Object_Bounds& bounds) const
{
	if (!m_pModel)
		return false;
	return World_Object_Bounds::IsCollision(m_bounds, bounds);
}

}
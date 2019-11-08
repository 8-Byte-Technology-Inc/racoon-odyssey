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
	__ComputeModelBaseCenterAndSize();
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

void World_Object::__ComputeModelBaseCenterAndSize()
{
	const std::vector<RenderModel_Mesh>& meshes = m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	m_size = m_pModel->GetSize();
	m_center = m_pModel->GetCenter();
	m_offset.y = (m_size.y / 2.f);
}

void World_Object::__ComputeModelAnimCenterAndSize(s32 animID)
{
	const RenderModel_Anim* pAnim = m_pModel->GetAnim(animID);

	m_size = pAnim->m_bounds.m_size;
	m_center.y = pAnim->m_bounds.m_center.y;
	m_offset.y = (pAnim->m_bounds.m_size.y / 2);
}

void World_Object::__InterpolateCenterAndSize(s32 animID0, s32 animID1, f32 t)
{
	const RenderModel_Anim* pAnimA = m_pModel->GetAnim(animID0);
	const RenderModel_Anim* pAnimB = m_pModel->GetAnim(animID1);

	const Vector3& centerA = pAnimA ? pAnimA->m_bounds.m_center : m_pModel->GetCenter();
	const Vector3& centerB = pAnimB ? pAnimB->m_bounds.m_center : m_pModel->GetCenter();

	const Vector3& sizeA = pAnimA ? pAnimA->m_bounds.m_size : m_pModel->GetSize();
	const Vector3& sizeB = pAnimB ? pAnimB->m_bounds.m_size : m_pModel->GetSize();

	const Vector3 center = (centerA * (1.f - t)) + (centerB * t);
	const Vector3 size = (sizeA * (1.f - t)) + (sizeB * t);

	m_size = size;
	m_center.y = center.y;
	m_offset.y = (size.y / 2);
}

void World_Object::__ComputeModelWorldLocalTransform()
{
	const std::vector<RenderModel_Mesh>& meshes = m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	// center it and apply offset, still in directX coords.
	Matrix4 matrixCenter;
	matrixCenter.SetTranslation(Vector3(-m_center.x, -m_center.y, -m_center.z));

	// align size of the model.
	Vector3 sizeAligned = m_size * m_scale;
	__GetRenderer()->AlignWorldSize(sizeAligned);

	// scale it.
	const Vector3 scale(is_approx_zero(m_size.x) ? 1.f : (sizeAligned.x / m_size.x),
		is_approx_zero(m_size.y) ? 1.f : (sizeAligned.y / m_size.y),
		is_approx_zero(m_size.z) ? 1.f : (sizeAligned.z / m_size.z));

	Matrix4 matrixScale;
	matrixScale.SetScale(scale);

	// offset.
	Matrix4 matrixOffset;
	matrixOffset.SetTranslation(Vector3(0.f, m_offset.y * scale.y, 0.f));

	// translate from directX to world coordinate system.
	Matrix4 matrixCoordsToWorld;
	matrixCoordsToWorld.SetIdentity();
	matrixCoordsToWorld.m[1][1] = 0.0f;
	matrixCoordsToWorld.m[2][1] = -1.0f;
	matrixCoordsToWorld.m[2][2] = 0.0f;
	matrixCoordsToWorld.m[1][2] = 1.0f;

	m_worldLocalTransform = matrixCoordsToWorld;
	m_worldLocalTransform = Matrix4::MultiplyAB(m_worldLocalTransform, matrixOffset);
	m_worldLocalTransform = Matrix4::MultiplyAB(m_worldLocalTransform, matrixScale);
	m_worldLocalTransform = Matrix4::MultiplyAB(m_worldLocalTransform, matrixCenter);
}

void World_Object::__SetAnim(s32 animID)
{
	for (u32 iJoint = 0; iJoint < m_pModel->GetJointCount(); ++iJoint)
	{
		const RenderModel_Anim_Joint* pJointAnimA = m_pModel->GetAnimJoint(animID, iJoint);
		const RenderModel_Joint* pJoint = m_pModel->GetJoint(iJoint);
		m_pModel->SetJointTransformMatrix(iJoint, pJointAnimA ? pJointAnimA->m_transform : pJoint->m_baseMatrix);
	}
}

void World_Object::__InterpolateAnims(s32 animID0, s32 animID1, f32 t)
{
	for (u32 iJoint = 0; iJoint < m_pModel->GetJointCount(); ++iJoint)
	{
		const RenderModel_Anim_Joint* pJointAnimA = m_pModel->GetAnimJoint(animID0, iJoint);
		const RenderModel_Anim_Joint* pJointAnimB = m_pModel->GetAnimJoint(animID1, iJoint);
		const RenderModel_Joint* pJoint = m_pModel->GetJoint(iJoint);
		if (!pJointAnimA && !pJointAnimB)
		{
			m_pModel->SetJointTransformMatrix(iJoint, pJoint->m_baseMatrix);
		}
		else
		{
			const Matrix4& matrixA = pJointAnimA ? pJointAnimA->m_transform : pJoint->m_baseMatrix;
			const Matrix4& matrixB = pJointAnimB ? pJointAnimB->m_transform : pJoint->m_baseMatrix;

			const Vector4 qA = Matrix4::ToQuaternion(matrixA);
			const Vector4 qB = Matrix4::ToQuaternion(matrixB);

			const Vector4 qR = Vector4::SLERP(qA, qB, t);

			Matrix4 matrixC = Matrix4::FromQuaternion(qR);

			Vector3 posA;
			Vector3 posB;
			matrixA.GetTranslation(posA);
			matrixB.GetTranslation(posB);

			Vector3 posC = (posA + posB) / 2.f;
			matrixC.AddTranslation(posC);

			m_pModel->SetJointTransformMatrix(iJoint, matrixC);
		}
	}
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
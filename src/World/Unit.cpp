#include "pch.h"

#include <cmath>

#include "render/RenderModel.h"

#include "Unit.h"

namespace TB8
{

World_Unit* World_Unit::Alloc(Client_Globals* pGlobalState)
{
	World_Unit* pObj = TB8_NEW(World_Unit)(pGlobalState);
	return pObj;
}

void World_Unit::Free()
{
	TB8_DEL(this);
}

void World_Unit::ComputeNextPosition(s32 frameCount, Vector3& pos, Vector3& vel)
{
	// compute environmental forces.
	const f32 forceFactor = 20.f;
	const f32 forceDampen = 10.0f;
	const f32 windResistFactor = 0.25f;
	const f32 gravityFactor = 10.0f;
	const f32 elapsedTime = static_cast<f32>(frameCount) / 60.f;
	const f32 velocityMax = 5.f; // m/s.

	// process jumping.
	if (!is_approx_zero(m_force.z))
	{
		if (is_approx_zero(m_pos.z))
		{
			m_velocity.z = +4.f;
		}
		m_force.z = 0.f;
	}

	const Vector3 forceChar = m_force * forceFactor;

	const f32 velTotalSq = m_velocity.MagSq();

	Vector3 forceEnv;
	forceEnv.x = (forceDampen + (velTotalSq * windResistFactor)) * get_sign(m_velocity.x) * -1.f;
	forceEnv.y = (forceDampen + (velTotalSq * windResistFactor)) * get_sign(m_velocity.y) * -1.f;
	forceEnv.z = (m_mass * gravityFactor) * -1.f;

	// compute net force.
	const Vector3 forceNet = forceChar + forceEnv;

	// compute accel.
	const Vector3 accel = forceNet / m_mass;

	// compute new velocity.
	vel = m_velocity + (accel * elapsedTime);

	// don't exceed max velocity.
	vel.x = std::min<f32>(abs(vel.x), velocityMax) * get_sign(vel.x);
	vel.y = std::min<f32>(abs(vel.y), velocityMax) * get_sign(vel.y);
	vel.z = std::min<f32>(abs(vel.z), velocityMax) * get_sign(vel.z);

	if (is_approx_zero(forceChar.x))
	{
		if (is_approx_zero(m_velocity.x))
			vel.x = 0.f;
		else if ((vel.x < 0.f) && (m_velocity.x > 0.f))
			vel.x = 0.f;
		else if ((vel.x > 0.f) && (m_velocity.x < 0.f))
			vel.x = 0.f;
	}

	if (is_approx_zero(forceChar.y))
	{
		if (is_approx_zero(m_velocity.y))
			vel.y = 0.f;
		else if ((vel.y < 0.f) && (m_velocity.y > 0.f))
			vel.y = 0.f;
		else if ((vel.y > 0.f) && (m_velocity.y < 0.f))
			vel.y = 0.f;
	}

	const Vector3& vel0 = m_velocity;
	const Vector3& vel1 = vel;

	// compute new position.
	pos = m_pos + (vel0 + ((vel1 - vel0) * 0.5f)) * elapsedTime;

	// can't go lower than 0.
	if (pos.z < 0.f)
	{
		pos.z = 0.f;
		vel.z = 0.f;
	}
}

void World_Unit::UpdatePosition(const Vector3& pos, const Vector3& vel)
{
	// compute new facing.
	f32 facing = m_rotation;
	if (!is_approx_zero(vel.x) || !is_approx_zero(vel.y))
	{
		Vector2 facingVector(vel.x, vel.y);
		facingVector.Normalize();
		const f32 angle = std::atan2(facingVector.y, facingVector.x);
		facing = (angle / DirectX::XM_PI) * 180.f;
	}

	// compute distance traveled.
	const Vector3 vDist = pos - m_pos;
	const f32 dist = vDist.Mag();

	// compute new animation.
	if (!is_approx_zero(dist))
	{
		// udpate anim index.
		m_animIndex += dist;
		while (m_animIndex > 1.f)
			m_animIndex -= 1.f;

		// no longer sitting.
		if (m_isSitting)
		{
			m_isSitting = false;
			m_offset = Vector3(0.f, 0.f, 0.25f);
			__ComputeModelWorldLocalTransform();
		}

		// decide which two animations we'll interpolate between.
		u32 animCount = m_pModel->GetAnimCount();
		const f32 animRange = static_cast<f32>(animCount);
		const u32 animIndex0 = static_cast<u32>(m_animIndex * animRange) % animCount;
		const u32 animIndex1 = (animIndex0 + 1) % animCount;
		const f32 t = (m_animIndex - (static_cast<f32>(animIndex0) / animRange)) * animRange;

		const RenderModel_Anim* pAnimA = m_pModel->GetAnim(animIndex0 + 1);
		const RenderModel_Anim* pAnimB = m_pModel->GetAnim(animIndex1 + 1);

		std::vector<RenderModel_Anim_Joint>::const_iterator itJointA = pAnimA->m_joints.begin();
		std::vector<RenderModel_Anim_Joint>::const_iterator itJointB = pAnimB->m_joints.begin();

		// update any joints that moved.
		for ( ; itJointA != pAnimA->m_joints.end() && itJointB != pAnimB->m_joints.end(); ++itJointA, ++itJointB)
		{
			const RenderModel_Anim_Joint& jointA = *itJointA;
			const RenderModel_Anim_Joint& jointB = *itJointB;

			const Matrix4& matrixA = jointA.m_transform;
			const Matrix4& matrixB = jointB.m_transform;

			Vector4 qA = Matrix4::ToQuaternion(matrixA);
			Vector4 qB = Matrix4::ToQuaternion(matrixB);

			Vector4 qR = Vector4::SLERP(qA, qB, t);

			Matrix4 matrixC = Matrix4::FromQuaternion(qR);

			Vector3 posA;
			Vector3 posB;
			matrixA.GetTranslation(posA);
			matrixB.GetTranslation(posB);

			Vector3 posC = (posA + posB) / 2.f;
			matrixC.AddTranslation(posC);

			m_pModel->SetJointTransformMatrix(jointA.m_pJoint->m_index, matrixC);
		}
	}

	// update position, rotation, velocity.
	m_pos = pos;
	m_rotation = facing;
	m_velocity = vel;

	// compute updated bounds.
	ComputeBounds();
}


}

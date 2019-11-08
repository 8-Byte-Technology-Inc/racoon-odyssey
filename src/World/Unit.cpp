#include "pch.h"

#include <cmath>

#include "render/RenderModel.h"

#include "Unit.h"

namespace TB8
{

const s32 SITTING_FRAME_MAX = 30;

World_Unit* World_Unit::Alloc(Client_Globals* pGlobalState)
{
	World_Unit* pObj = TB8_NEW(World_Unit)(pGlobalState);
	pObj->__Initialize();
	return pObj;
}

void World_Unit::Free()
{
	TB8_DEL(this);
}

void World_Unit::__Initialize()
{
	m_sittingFrame = SITTING_FRAME_MAX;
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

void World_Unit::UpdatePosition(s32 frameCount, const Vector3& pos, const Vector3& vel)
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
		if (m_sittingFrame > 0)
		{
			m_sittingFrame -= (frameCount * 2);

			if (m_sittingFrame > 0)
			{
				u32 animCount = m_pModel->GetAnimCount();
				const f32 animRange = static_cast<f32>(animCount);
				const u32 animIndex0 = static_cast<u32>(m_animIndex * animRange) % animCount;
				const f32 t = static_cast<f32>(SITTING_FRAME_MAX - m_sittingFrame) / static_cast<f32>(SITTING_FRAME_MAX);

				__InterpolateAnims(-1, 1, t);
				__InterpolateCenterAndSize(-1, 1, t);
				__ComputeModelWorldLocalTransform();
				return;
			}

			m_sittingFrame = 0;
			m_animIndex = 0.f;

			__ComputeModelAnimCenterAndSize(1);
			__ComputeModelWorldLocalTransform();
		}

		// udpate anim index.
		m_animIndex += dist;
		while (m_animIndex > 1.f)
			m_animIndex -= 1.f;

		// decide which two animations we'll interpolate between.
		u32 animCount = m_pModel->GetAnimCount();
		const f32 animRange = static_cast<f32>(animCount);
		const u32 animIndex0 = static_cast<u32>(m_animIndex * animRange) % animCount;
		const u32 animIndex1 = (animIndex0 + 1) % animCount;
		const f32 t = (m_animIndex - (static_cast<f32>(animIndex0) / animRange)) * animRange;

		__InterpolateAnims(animIndex0 + 1, animIndex1 + 1, t);
	}
	else if (m_sittingFrame < SITTING_FRAME_MAX)
	{
		// no longer moving, sit down.
		m_sittingFrame += frameCount;

		if (m_sittingFrame < SITTING_FRAME_MAX)
		{
			u32 animCount = m_pModel->GetAnimCount();
			const f32 animRange = static_cast<f32>(animCount);
			const u32 animIndex0 = static_cast<u32>(m_animIndex * animRange) % animCount;
			const f32 t = static_cast<f32>(m_sittingFrame) / static_cast<f32>(SITTING_FRAME_MAX);

			__InterpolateAnims(animIndex0 + 1, -1, t);
			__InterpolateCenterAndSize(animIndex0 + 1, -1, t);
			__ComputeModelWorldLocalTransform();
			return;
		}

		m_sittingFrame = SITTING_FRAME_MAX;
		m_animIndex = 0.f;

		__SetAnim(-1);
		__ComputeModelAnimCenterAndSize(-1);
		__ComputeModelWorldLocalTransform();
	}

	// update position, rotation, velocity.
	m_pos = pos;
	m_rotation = facing;
	m_velocity = vel;

	// compute updated bounds.
	ComputeBounds();
}


}

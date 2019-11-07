#pragma once

#include "common/basic_types.h"

#include "Object.h"

namespace TB8
{

struct World_Unit : public World_Object
{
	World_Unit(Client_Globals* pGlobalState)
		: World_Object(pGlobalState)
		, m_mass(0.f)
		, m_animIndex(0.f)
		, m_isSitting(true)
	{
	}

	static World_Unit* Alloc(Client_Globals* pGlobalState);
	virtual void Free() override;

	void ComputeNextPosition(s32 frameCount, Vector3& pos, Vector3& vel);
	void UpdatePosition(const Vector3& pos, const Vector3& vel);
	const Vector3& GetForce() const { return m_force; }
	void SetForce(const Vector3& force) { m_force = force; }

	f32								m_mass;
	Vector3							m_force;
	Vector3							m_velocity;

	f32								m_animIndex;
	bool							m_isSitting;
};

}
#pragma once

#include "common/basic_types.h"

#include "Object.h"

namespace TB8
{

class RenderImagine;

struct World_Unit : public World_Object
{
	World_Unit(Client_Globals* pGlobalState)
		: World_Object(pGlobalState)
		, m_mass(0.f)
		, m_maxVelocity(0.f)
		, m_distanceTravelled(0.f)
		, m_animIndex(0.f)
		, m_sittingFrame(0)
		, m_thoughtFrame(0)
		, m_pImagine(nullptr)
	{
	}
	~World_Unit();

	static World_Unit* Alloc(Client_Globals* pGlobalState);
	virtual void Free() override;

	void ComputeNextPosition(s32 frameCount, Vector3& pos, Vector3& vel);
	void UpdatePosition(s32 frameCount, const Vector3& pos, const Vector3& vel);
	virtual void Update(s32 frameCount);
	const Vector3& GetForce() const { return m_force; }
	void SetForce(const Vector3& force) { m_force = force; }
	bool IsSitting() const;

	virtual void Render2D(const Vector3& screenWorldPos) override;

	void __Initialize();

	f32								m_mass;
	f32								m_maxVelocity;
	Vector3							m_force;
	Vector3							m_velocity;

	f32								m_distanceTravelled;

	f32								m_animIndex;
	s32								m_sittingFrame;
	s32								m_thoughtFrame;
	s32								m_thoughtFrameNext;
	std::vector<u32>				m_thoughtFrequency;

	RenderImagine*					m_pImagine;
};

}
#pragma once

#include "common/basic_types.h"

#include "Unit.h"

namespace TB8
{

class RenderStatusBars;

struct World_Avatar : public World_Unit
{
	World_Avatar(Client_Globals* pGlobalState);
	~World_Avatar();

	static World_Avatar* Alloc(Client_Globals* pGlobalState, const char* pszCharacterModelPath);
	virtual void Free() override;

	virtual void Update(s32 frameCount) override;
	virtual void Render2D(const Vector3& screenWorldPos) override;
	virtual void Render3D(const Vector3& screenWorldPos) override;

	void __Initialize(const char* pszCharacterModelPath);
	void __Uninitialize();
	void __UpdateVelocity();

	RenderStatusBars*	m_pStatusBars;

	u32					m_barID_Chill;
	u32					m_barID_Spunk;
	u32					m_barID_Full;
	u32					m_barID_Gas;

	u32					m_restingFrameCount;
	u32					m_digestingFrameCount;
	f32					m_distanceTravelledLast;
};

}


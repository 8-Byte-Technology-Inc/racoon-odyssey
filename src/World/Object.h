#pragma once

#include "common/basic_types.h"

#include "client/Client_Globals.h"

#include "Bounds.h"

namespace TB8
{

class RenderModel;

struct World_Object : public Client_Globals_Accessor
{
	World_Object(Client_Globals* pGlobalState)
		: Client_Globals_Accessor(pGlobalState)
		, m_modelID(0)
		, m_pModel(nullptr)
		, m_offset()
		, m_pos()
		, m_rotation(0.f)
		, m_scale(0.f)
		, m_worldLocalTransform()
		, m_bounds()
	{
	}

	static World_Object* Alloc(Client_Globals* pGlobalState);
	virtual void Free();

	void Init();
	void ComputeBounds();
	bool IsCollision(World_Object_Bounds& bounds) const;

	virtual void Render(const Vector3& screenWorldPos);

	void __ComputeModelWorldLocalTransform();

	u32								m_modelID;
	RenderModel*					m_pModel;
	Vector3							m_offset;
	Vector3							m_pos;
	f32								m_rotation;
	f32								m_scale;
	Matrix4							m_worldLocalTransform;
	World_Object_Bounds				m_bounds;
};

}

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
		, m_center()
		, m_offset()
		, m_size()
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

	void __ComputeModelBaseCenterAndSize();
	void __ComputeModelAnimCenterAndSize(s32 animID);
	void __ComputeModelWorldLocalTransform();
	void __InterpolateCenterAndSize(s32 animID0, s32 animID1, f32 t);
	void __SetAnim(s32 animID0);
	void __InterpolateAnims(s32 animID0, s32 animID1, f32 t);

	u32								m_modelID;
	RenderModel*					m_pModel;
	Vector3							m_center;
	Vector3							m_offset;
	Vector3							m_size;
	Vector3							m_pos;
	f32								m_rotation;
	f32								m_scale;
	Matrix4							m_worldLocalTransform;
	World_Object_Bounds				m_bounds;
};

}

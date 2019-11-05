#pragma once

#include "common/basic_types.h"

namespace TB8
{

struct World_Object;

enum World_Object_Bounds_Type : u32
{
	World_Object_Bounds_Type_None,
	World_Object_Bounds_Type_Box,
	World_Object_Bounds_Type_Sphere,
};

struct World_Object_Bounds
{
	World_Object_Bounds()
		: m_type(World_Object_Bounds_Type_None)
		, m_radius(0.f)
	{
	}

	void ComputeBounds(World_Object& object);
	void __ComputeBoundsBox(World_Object& object);
	void __ComputeBoundsSphere(World_Object& object);

	static bool IsCollision(const World_Object_Bounds& boundsA, const World_Object_Bounds& boundsB);
	static bool IsCollisionAlongNormal(const Vector3& projection, const World_Object_Bounds& boundsA, const World_Object_Bounds& boundsB);

	World_Object_Bounds_Type		m_type;
	Vector3							m_center;
	Vector3							m_coords[8];
	f32								m_radius;
};

}
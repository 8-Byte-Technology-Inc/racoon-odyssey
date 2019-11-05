#include "pch.h"

#include <cmath>

#include "render/RenderModel.h"

#include "Bounds.h"
#include "Object.h"

namespace TB8
{

void World_Object_Bounds::ComputeBounds(World_Object& object)
{
	switch (m_type)
	{
	case World_Object_Bounds_Type_Box:
		__ComputeBoundsBox(object);
		break;
	case World_Object_Bounds_Type_Sphere:
		__ComputeBoundsSphere(object);
		break;
	default:
		break;
	}
}

void World_Object_Bounds::__ComputeBoundsBox(World_Object& object)
{
	const std::vector<RenderModel_Mesh>& meshes = object.m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	// rotate it.
	Matrix4 matrixRotate;
	matrixRotate.SetRotate(Vector3(0.f, 0.f, DirectX::XM_PI * (object.m_rotation) / 180.f));

	// position it.
	Matrix4 matrixPosition;
	matrixPosition.SetTranslation(object.m_pos);

	// compute the overall transform.
	Matrix4 worldTransform = matrixPosition;
	worldTransform = Matrix4::MultiplyAB(worldTransform, matrixRotate);
	worldTransform = Matrix4::MultiplyAB(worldTransform, object.m_worldLocalTransform);

	m_type = World_Object_Bounds_Type_Box;
	m_center = Vector3((mesh.m_max.x + mesh.m_min.x) / 2.f, (mesh.m_max.y + mesh.m_min.y) / 2.f, (mesh.m_max.z + mesh.m_min.z) / 2.f);
	m_coords[0] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_min.z);
	m_coords[1] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_min.z);
	m_coords[2] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_min.z);
	m_coords[3] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_min.z);
	m_coords[4] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_max.z);
	m_coords[5] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_max.z);
	m_coords[6] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_max.z);
	m_coords[7] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_max.z);

	m_center = Matrix4::MultiplyVector(m_center, worldTransform);
	for (u32 i = 0; i < ARRAYSIZE(m_coords); ++i)
	{
		m_coords[i] = Matrix4::MultiplyVector(m_coords[i], worldTransform);
	}
}

void World_Object_Bounds::__ComputeBoundsSphere(World_Object& object)
{
	const std::vector<RenderModel_Mesh>& meshes = object.m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	const Vector3 vCenter((mesh.m_max.x + mesh.m_min.x) / 2.f, (mesh.m_max.y + mesh.m_min.y) / 2.f, (mesh.m_max.z + mesh.m_min.z) / 2.f);
	const f32 radius = std::max<f32>(std::max<f32>(mesh.m_max.x - mesh.m_min.x, mesh.m_max.y - mesh.m_min.y), mesh.m_max.z - mesh.m_min.z) / 2.f;

	m_type = World_Object_Bounds_Type_Sphere;
	m_center = Matrix4::MultiplyVector(vCenter, object.m_worldLocalTransform) + object.m_pos;
	m_radius = radius * object.m_scale;
}

bool World_Object_Bounds::IsCollision(const World_Object_Bounds& boundsA, const World_Object_Bounds& boundsB)
{
	if ((boundsA.m_type == World_Object_Bounds_Type_None)
		|| (boundsB.m_type == World_Object_Bounds_Type_None))
		return false;

	if (boundsA.m_type == World_Object_Bounds_Type_Box)
	{
		const Vector3 n1 = Vector3::ComputeNormal(boundsA.m_coords[0], boundsA.m_coords[1], boundsA.m_coords[3]);
		if (!IsCollisionAlongNormal(n1, boundsA, boundsB))
			return false;
		const Vector3 n2 = Vector3::ComputeNormal(boundsA.m_coords[1], boundsA.m_coords[2], boundsA.m_coords[5]);
		if (!IsCollisionAlongNormal(n2, boundsA, boundsB))
			return false;
		const Vector3 n3 = Vector3::ComputeNormal(boundsA.m_coords[0], boundsA.m_coords[4], boundsA.m_coords[1]);
		if (!IsCollisionAlongNormal(n3, boundsA, boundsB))
			return false;
	}

	if (boundsB.m_type == World_Object_Bounds_Type_Box)
	{
		const Vector3 n1 = Vector3::ComputeNormal(boundsB.m_coords[0], boundsB.m_coords[1], boundsB.m_coords[3]);
		if (!IsCollisionAlongNormal(n1, boundsA, boundsB))
			return false;
		const Vector3 n2 = Vector3::ComputeNormal(boundsB.m_coords[1], boundsB.m_coords[2], boundsB.m_coords[5]);
		if (!IsCollisionAlongNormal(n2, boundsA, boundsB))
			return false;
		const Vector3 n3 = Vector3::ComputeNormal(boundsB.m_coords[0], boundsB.m_coords[4], boundsB.m_coords[1]);
		if (!IsCollisionAlongNormal(n3, boundsA, boundsB))
			return false;
	}

	if ((boundsA.m_type == World_Object_Bounds_Type_Sphere)
		&& (boundsB.m_type == World_Object_Bounds_Type_Sphere))
	{
		const Vector3 AB = boundsB.m_center - boundsA.m_center;
		const Vector3 n = Vector3::Normalize(AB);
		if (!IsCollisionAlongNormal(n, boundsA, boundsB))
			return false;
	}

	return true;
}

bool World_Object_Bounds::IsCollisionAlongNormal(const Vector3& normal, const World_Object_Bounds& boundsA, const World_Object_Bounds& boundsB)
{
	u32 cProjA = 0;
	u32 cProjB = 0;
	f32 projA[8];
	f32 projB[8];

	const Vector3 centerToCenter(boundsB.m_center.x - boundsA.m_center.x, boundsB.m_center.y - boundsA.m_center.y, boundsB.m_center.z - boundsA.m_center.z);
	const f32 projCenter = Vector3::Dot(centerToCenter, normal);

	if (boundsA.m_type == World_Object_Bounds_Type_Box)
	{
		assert(ARRAYSIZE(boundsA.m_coords) <= ARRAYSIZE(projA));
		for (u32 i = 0; i < ARRAYSIZE(boundsA.m_coords); ++i)
		{
			projA[i] = Vector3::Dot(boundsA.m_coords[i] - boundsA.m_center, normal);
		}
		cProjA = ARRAYSIZE(boundsA.m_coords);
		std::sort(projA, projA + cProjA);
	}
	else if (boundsA.m_type == World_Object_Bounds_Type_Sphere)
	{
		assert(2 <= ARRAYSIZE(projA));
		projA[0] = -boundsA.m_radius;
		projA[1] = +boundsA.m_radius;
		cProjA = 2;
	}

	if (boundsB.m_type == World_Object_Bounds_Type_Box)
	{
		assert(ARRAYSIZE(boundsB.m_coords) <= ARRAYSIZE(projB));
		for (u32 i = 0; i < ARRAYSIZE(boundsB.m_coords); ++i)
		{
			projB[i] = Vector3::Dot(boundsB.m_coords[i] - boundsB.m_center, normal);
		}
		cProjB = ARRAYSIZE(boundsB.m_coords);
		std::sort(projB, projB + cProjB);
	}
	else if (boundsB.m_type == World_Object_Bounds_Type_Sphere)
	{
		assert(2 <= ARRAYSIZE(projB));
		projB[0] = -boundsB.m_radius;
		projB[1] = +boundsB.m_radius;
		cProjB = 2;
	}

	if (projCenter < 0.f)
	{
		const f32 dist = (-1.f * projCenter) - projB[cProjB - 1] + projA[0];
		return dist < 0.f;
	}
	else
	{
		const f32 dist = projCenter - projA[cProjA - 1] + projB[0];
		return dist < 0.f;
	}
}

}
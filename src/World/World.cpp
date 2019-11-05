#include "pch.h"

#include <cmath>

#include "common/memory.h"
#include "common/parse_xml.h"
#include "common/string.h"

#include "event/EventQueue.h"
#include "event/EventQueueMessages.h"

#include "render/RenderMain.h"
#include "render/RenderModel.h"

#include "World.h"

namespace TB8
{

const f32 TILES_PER_METER = 1.0f;

World::World(Client_Globals* pGlobalState)
	: Client_Globals_Accessor(pGlobalState)
	, m_characterMass(2.f)
{
}

World::~World()
{
	__Uninitialize();
}

World* World::Alloc(Client_Globals* pGlobalState)
{
	World* pWorld = TB8_NEW(World)(pGlobalState);
	pWorld->__Initialize();
	return pWorld;
}

void World::Free()
{
	TB8_DEL(this);
}

void World::LoadMap(const char* pszMapPath)
{
	std::string mapPathFile = __GetPathAssets();
	TB8::File::AppendToPath(mapPathFile, pszMapPath);

	m_mapPath = mapPathFile;
	File::StripFileNameFromPath(m_mapPath);

	// open the xml file.
	TB8::File* f = TB8::File::AllocOpen(mapPathFile.c_str(), true);
	assert(f);

	XML_Parser parser;
	parser.SetHandlers(std::bind(&World::__ParseMapStartElement, this, std::placeholders::_1, std::placeholders::_2),
						std::bind(&World::__ParseMapCharacters, this, std::placeholders::_1, std::placeholders::_2),
						std::bind(&World::__ParseMapEndElement, this, std::placeholders::_1));
	parser.SetReader(std::bind(__ParseMapRead, f, std::placeholders::_1, std::placeholders::_2));
	parser.Parse();
}

void World::LoadCharacter(const char* pszCharacterModelPath, const char* pszModelName)
{
	RenderModel* pModel = RenderModel::AllocFromDAE(__GetRenderer(), __GetPathAssets().c_str(), pszCharacterModelPath, pszModelName);
	m_mapModels.insert(std::make_pair(0, pModel));

	const std::vector<RenderModel_Mesh>& meshes = pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	m_characterModel.m_modelID = 0;
	m_characterModel.m_pModel = pModel;
	// scale the model so <y> is 0.75 meters.
	m_characterModel.m_scale = .75f / mesh.m_size.y;

	__ComputeModelWorldLocalTransform(m_characterModel);
	__ComputeCharacterModelSphere(m_characterModel, &(m_characterModel.m_bounds));
}

void World::Update(s32 frameCount)
{
	// compute environmental forces.
	const f32 forceFactor = 20.f;
	const f32 forceDampen = 10.0f;
	const f32 windResistFactor = 0.25f;
	const f32 gravityFactor = 10.0f;
	const f32 elapsedTime = static_cast<f32>(frameCount) / 60.f;
	const f32 velocityMax = 5.f; // m/s.

	// process jumping.
	if (!is_approx_zero(m_characterForce.z))
	{
		if (is_approx_zero(m_characterModel.m_pos.z))
		{
			m_characterVelocity.z = +4.f;
		}
		m_characterForce.z = 0.f;
	}

	const Vector3 forceChar = m_characterForce * forceFactor;

	const f32 velTotalSq = m_characterVelocity.MagSq();

	Vector3 forceEnv;
	forceEnv.x = (forceDampen + (velTotalSq * windResistFactor)) * get_sign(m_characterVelocity.x) * -1.f;
	forceEnv.y = (forceDampen + (velTotalSq * windResistFactor)) * get_sign(m_characterVelocity.y) * -1.f;
	forceEnv.z = (m_characterMass * gravityFactor) * -1.f;

	// compute net force.
	const Vector3 forceNet = forceChar + forceEnv;

	// compute accel.
	const Vector3 accel = forceNet / m_characterMass;

	// compute new velocity.
	Vector3 vel = m_characterVelocity + (accel * elapsedTime);

	// don't exceed max velocity.
	vel.x = std::min<f32>(abs(vel.x), velocityMax) * get_sign(vel.x);
	vel.y = std::min<f32>(abs(vel.y), velocityMax) * get_sign(vel.y);
	vel.z = std::min<f32>(abs(vel.z), velocityMax) * get_sign(vel.z);

	if (is_approx_zero(forceChar.x))
	{
		if (is_approx_zero(m_characterVelocity.x))
			vel.x = 0.f;
		else if ((vel.x < 0.f) && (m_characterVelocity.x > 0.f))
			vel.x = 0.f;
		else if ((vel.x > 0.f) && (m_characterVelocity.x < 0.f))
			vel.x = 0.f;
	}

	if (is_approx_zero(forceChar.y))
	{
		if (is_approx_zero(m_characterVelocity.y))
			vel.y = 0.f;
		else if ((vel.y < 0.f) && (m_characterVelocity.y > 0.f))
			vel.y = 0.f;
		else if ((vel.y > 0.f) && (m_characterVelocity.y < 0.f))
			vel.y = 0.f;
	}

	const Vector3& vel0 = m_characterVelocity;
	const Vector3& vel1 = vel;

	// compute new position.
	Vector3 pos = m_characterModel.m_pos + (vel0 + ((vel1 - vel0) * 0.5f)) * elapsedTime;

	// compute new facing.
	f32 facing = m_characterModel.m_rotation;
	if (!is_approx_zero(vel1.x) || !is_approx_zero(vel1.y))
	{
		Vector2 facingVector(vel1.x, vel1.y);
		facingVector.Normalize();
		const f32 angle = std::atan2(facingVector.y, facingVector.x);
		facing = (angle / DirectX::XM_PI) * 180.f;
	}

	// can't go lower than 0.
	if (pos.z < 0.f)
	{
		pos.z = 0.f;
		vel.z = 0.f;
	}

	// adjust position & velocity for collisions.
	__AdjustCharacterModelPositionForCollisions(pos, vel);

	// update.
	m_characterModel.m_pos = pos;
	m_characterModel.m_rotation = facing;
	m_characterVelocity = vel;
	__ComputeCharacterModelSphere(m_characterModel, &(m_characterModel.m_bounds));
}

void World::Render(RenderMain* pRenderer)
{

	// draw all the tiles that might be on-screen.
	const Vector2& screenSizeWorld = pRenderer->GetRenderScreenSizeWorld();

	// compute which tiles we'll draw.
	IRect tiles;
	tiles.left = static_cast<u32>((m_characterModel.m_pos.x - ((screenSizeWorld.x / 2.f) * 3.f) / TILES_PER_METER));
	tiles.right = static_cast<u32>((m_characterModel.m_pos.x + ((screenSizeWorld.x / 2.f) * 3.f) / TILES_PER_METER)) + 1;
	tiles.top = static_cast<u32>((m_characterModel.m_pos.y - ((screenSizeWorld.y / 2.f) * 4.f) / TILES_PER_METER));
	tiles.bottom = static_cast<u32>((m_characterModel.m_pos.y + ((screenSizeWorld.y / 2.f) * 4.f) / TILES_PER_METER)) + 1;

	Vector3 screenWorldPos = m_characterModel.m_pos;
	__GetRenderer()->AlignWorldPosition(screenWorldPos);

	// draw the tiles & walls.
	IVector2 cellPos;
	for (cellPos.x = tiles.left; cellPos.x < tiles.right; ++cellPos.x)
	{
		for (cellPos.y = tiles.top; cellPos.y < tiles.bottom; ++cellPos.y)
		{
			MapCell& cell = __GetCell(cellPos);
			{
				MapModel& model = cell.m_tile;
				if (model.m_pModel)
				{
					Matrix4 worldTransform;
					__ComputeModelWorldTransformRender(model, screenWorldPos, &worldTransform);
					model.m_pModel->SetWorldTransform(worldTransform);
					model.m_pModel->Render(pRenderer);
				}
			}

			for (u32 i = 0; i < ARRAYSIZE(cell.m_wall); ++i)
			{
				MapModel& model = cell.m_wall[i];
				if (model.m_pModel)
				{
					Matrix4 worldTransform;
					__ComputeModelWorldTransformRender(model, screenWorldPos, &worldTransform);
					model.m_pModel->SetWorldTransform(worldTransform);
					model.m_pModel->Render(pRenderer);
				}
			}
		}
	}

	// draw the character.
	{
		Matrix4 worldTransform;
		__ComputeModelWorldTransformRender(m_characterModel, m_characterModel.m_pos, &worldTransform);
		m_characterModel.m_pModel->SetWorldTransform(worldTransform);
		m_characterModel.m_pModel->Render(pRenderer);
	}
}

void World::__Initialize()
{
	// register for events.
	__GetEventQueue()->RegisterForMessage(EventModuleID_World, EventMessageID_MoveStart, std::bind(&World::__EventHandler, this, std::placeholders::_1));
	__GetEventQueue()->RegisterForMessage(EventModuleID_World, EventMessageID_MoveEnd, std::bind(&World::__EventHandler, this, std::placeholders::_1));
}

void World::__Uninitialize()
{
	__GetEventQueue()->UnregisterForMessagesByRegistree(EventModuleID_World);

	for (std::map<u32, RenderModel*>::iterator it = m_mapModels.begin(); it != m_mapModels.end(); ++it)
	{
		RELEASEI(it->second);
	}
}

void World::__EventHandler(EventMessage* pEvent)
{
	switch (pEvent->GetID())
	{
	case EventMessageID_MoveStart:
	{
		EventMessage_MoveStart* pEventMoveStart = static_cast<EventMessage_MoveStart*>(pEvent);
		if (pEventMoveStart->m_dx != 0)
		{
			m_characterForce.x = static_cast<f32>(pEventMoveStart->m_dx);
		}
		if (pEventMoveStart->m_dy != 0)
		{
			m_characterForce.y = static_cast<f32>(pEventMoveStart->m_dy);
		}
		if (pEventMoveStart->m_dz != 0)
		{
			m_characterForce.z = static_cast<f32>(pEventMoveStart->m_dz);
		}
		break;
	}
	case EventMessageID_MoveEnd:
	{
		EventMessage_MoveStart* pEventMoveEnd = static_cast<EventMessage_MoveStart*>(pEvent);
		if (pEventMoveEnd->m_dx != 0)
		{
			m_characterForce.x = 0.f;
		}
		if (pEventMoveEnd->m_dy != 0)
		{
			m_characterForce.y = 0.f;
		}
		if (pEventMoveEnd->m_dz != 0)
		{
			m_characterForce.z = 0.f;
		}
		break;
	}
	default:
		break;
	}
}

void World::__ComputeModelWorldLocalTransform(MapModel& model)
{
	const std::vector<RenderModel_Mesh>& meshes = model.m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	// center it, still in directX coords.
	Matrix4 matrixCenter;
	matrixCenter.SetTranslation(Vector3(0.f - ((mesh.m_max.x + mesh.m_min.x) / 2.f), 0.f - mesh.m_min.y, 0.f - ((mesh.m_max.z + mesh.m_min.z) / 2.f)));

	// align size of the model.
	const Vector3 sizeBase(mesh.m_max.x - mesh.m_min.x, mesh.m_max.y - mesh.m_min.y, mesh.m_max.z - mesh.m_min.z);
	Vector3 sizeAligned = sizeBase * model.m_scale;
	__GetRenderer()->AlignWorldSize(sizeAligned);

	// scale it.
	const Vector3 scale(is_approx_zero(sizeBase.x) ? 1.f : (sizeAligned.x / sizeBase.x), 
					is_approx_zero(sizeBase.y) ? 1.f : (sizeAligned.y / sizeBase.y), 
					is_approx_zero(sizeBase.z) ? 1.f : (sizeAligned.z / sizeBase.z));

	Matrix4 matrixScale;
	matrixScale.SetScale(scale);

	// translate from directX to world coordinate system.
	Matrix4 matrixCoordsToWorld;
	matrixCoordsToWorld.SetIdentity();
	matrixCoordsToWorld.m[1][1] = 0.0f;
	matrixCoordsToWorld.m[2][1] = -1.0f;
	matrixCoordsToWorld.m[2][2] = 0.0f;
	matrixCoordsToWorld.m[1][2] = 1.0f;

	model.m_worldLocalTransform = matrixCoordsToWorld;
	model.m_worldLocalTransform = Matrix4::MultiplyAB(model.m_worldLocalTransform, matrixScale);
	model.m_worldLocalTransform = Matrix4::MultiplyAB(model.m_worldLocalTransform, matrixCenter);
}

void World::__ComputeModelWorldTransformRender(MapModel& model, const Vector3& screenWorldPos, Matrix4* pWorldTransform)
{
	// rotate it.
	Matrix4 matrixRotate;
	matrixRotate.SetRotate(Vector3(0.f, 0.f, DirectX::XM_PI * (model.m_rotation) / 180.f));

	// position it.
	Vector3 renderPos = model.m_pos - screenWorldPos;

	Matrix4 matrixPosition;
	matrixPosition.SetTranslation(renderPos);

	// compute the overall transform.
	*pWorldTransform = matrixPosition;
	*pWorldTransform  = Matrix4::MultiplyAB(*pWorldTransform, matrixRotate);
	*pWorldTransform  = Matrix4::MultiplyAB(*pWorldTransform, model.m_worldLocalTransform);
}

void World::__ComputeCharacterModelBox(MapModel& model, MapModelBounds* pBounds)
{
	const std::vector<RenderModel_Mesh>& meshes = model.m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	// rotate it.
	Matrix4 matrixRotate;
	matrixRotate.SetRotate(Vector3(0.f, 0.f, DirectX::XM_PI * (model.m_rotation) / 180.f));

	// position it.
	Matrix4 matrixPosition;
	matrixPosition.SetTranslation(model.m_pos);

	// compute the overall transform.
	Matrix4 worldTransform = matrixPosition;
	worldTransform = Matrix4::MultiplyAB(worldTransform, matrixRotate);
	worldTransform = Matrix4::MultiplyAB(worldTransform, model.m_worldLocalTransform);

	pBounds->m_type = MapModelBounds_Type_Box;
	pBounds->m_center = Vector3((mesh.m_max.x + mesh.m_min.x) / 2.f, (mesh.m_max.y + mesh.m_min.y) / 2.f, (mesh.m_max.z + mesh.m_min.z) / 2.f);
	pBounds->m_coords[0] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_min.z);
	pBounds->m_coords[1] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_min.z);
	pBounds->m_coords[2] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_min.z);
	pBounds->m_coords[3] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_min.z);
	pBounds->m_coords[4] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_max.z);
	pBounds->m_coords[5] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_max.z);
	pBounds->m_coords[6] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_max.z);
	pBounds->m_coords[7] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_max.z);

	pBounds->m_center = Matrix4::MultiplyVector(pBounds->m_center, worldTransform);
	for (u32 i = 0; i < ARRAYSIZE(pBounds->m_coords); ++i)
	{
		pBounds->m_coords[i] = Matrix4::MultiplyVector(pBounds->m_coords[i], worldTransform);
	}
}

void World::__ComputeCharacterModelSphere(MapModel& model, MapModelBounds* pBounds)
{
	const std::vector<RenderModel_Mesh>& meshes = model.m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	const Vector3 vCenter((mesh.m_max.x + mesh.m_min.x) / 2.f, (mesh.m_max.y + mesh.m_min.y) / 2.f, (mesh.m_max.z + mesh.m_min.z) / 2.f);
	const f32 radius = std::max<f32>(std::max<f32>(mesh.m_max.x - mesh.m_min.x, mesh.m_max.y - mesh.m_min.y), mesh.m_max.z - mesh.m_min.z) / 2.f;

	pBounds->m_type = MapModelBounds_Type_Sphere;
	pBounds->m_center = Matrix4::MultiplyVector(vCenter, model.m_worldLocalTransform) + model.m_pos;
	pBounds->m_radius = radius * model.m_scale;
}

void World::__AdjustCharacterModelPositionForCollisions(Vector3& pos, Vector3& vel)
{
	if (!__IsCharacterModelCollideWithWall(pos))
		return;

	__AdjustCharacterModelPositionForCollisionsAxis(pos, vel, Vector3(1.f, 0.f, 0.f));
	__AdjustCharacterModelPositionForCollisionsAxis(pos, vel, Vector3(0.f, 1.f, 0.f));
	__AdjustCharacterModelPositionForCollisionsAxis(pos, vel, Vector3(0.f, 0.f, 1.f));
}

void World::__AdjustCharacterModelPositionForCollisionsAxis(Vector3& pos, Vector3& vel, const Vector3& axis)
{
	const MapModel& model = m_characterModel;
	const Vector3 posDelta = pos - model.m_pos;
	const f32 posDeltaAxisMag = Vector3::Dot(posDelta, axis);
	if (is_approx_zero(posDeltaAxisMag))
		return;
	if (!__IsCharacterModelCollideWithWall(model.m_pos + (axis * posDeltaAxisMag)))
		return;

	// this axis collides. interpolate position.
	f32 min = 0.f;
	f32 max = posDeltaAxisMag;

	for (u32 i = 0; i < 8; ++i)
	{
		const f32 mid = (max + min) / 2.f;
		if (__IsCharacterModelCollideWithWall(model.m_pos + (axis * mid)))
		{
			max = mid;
		}
		else
		{
			min = mid;
		}
	}

	// set revised position.
	if (!is_approx_zero(axis.x))
	{
		pos.x = model.m_pos.x + min;
		vel.x = 0.f;
	}
	if (!is_approx_zero(axis.y))
	{
		pos.y = model.m_pos.y + min;
		vel.y = 0.f;
	}
	if (!is_approx_zero(axis.z))
	{
		pos.z = model.m_pos.z + min;
		vel.z = 0.f;
	}
}


bool World::__IsCharacterModelCollideWithWall(const Vector3& posNew) const
{
	const MapModel& model = m_characterModel;
	const Vector3 posDelta = posNew - model.m_pos;

	MapModelBounds boundsNew = model.m_bounds;
	boundsNew.m_center = boundsNew.m_center + posDelta;

	// compute which tiles to examine for collisions.
	IRect tiles;
	tiles.left = static_cast<u32>(posNew.x / TILES_PER_METER) - 1;
	tiles.right = static_cast<u32>(posNew.x / TILES_PER_METER) + 1;
	tiles.top = static_cast<u32>(posNew.y / TILES_PER_METER) - 1;
	tiles.bottom = static_cast<u32>(posNew.y / TILES_PER_METER) + 1;

	// look at walls for each tile.
	IVector2 cellPos;
	for (cellPos.x = tiles.left; cellPos.x <= tiles.right; ++cellPos.x)
	{
		for (cellPos.y = tiles.top; cellPos.y <= tiles.bottom; ++cellPos.y)
		{
			const MapCell& cell = __GetCell(cellPos);
			for (u32 i = 0; i < ARRAYSIZE(cell.m_wall); ++i)
			{
				const MapModel& wallModel = cell.m_wall[i];
				if (!wallModel.m_pModel)
					continue;
				if (!__IsCollision(wallModel.m_bounds, boundsNew))
					continue;
				return true;
			}
		}
	}

	return false;
}

bool World::__IsCollision(const MapModelBounds& boundsA, const MapModelBounds& boundsB) const
{
	if (boundsA.m_type == MapModelBounds_Type_Box)
	{
		const Vector3 n1 = Vector3::ComputeNormal(boundsA.m_coords[0], boundsA.m_coords[1], boundsA.m_coords[3]);
		if (!__IsCollisionAlongNormal(n1, boundsA, boundsB))
			return false;
		const Vector3 n2 = Vector3::ComputeNormal(boundsA.m_coords[1], boundsA.m_coords[2], boundsA.m_coords[5]);
		if (!__IsCollisionAlongNormal(n1, boundsA, boundsB))
			return false;
		const Vector3 n3 = Vector3::ComputeNormal(boundsA.m_coords[0], boundsA.m_coords[4], boundsA.m_coords[1]);
		if (!__IsCollisionAlongNormal(n1, boundsA, boundsB))
			return false;
	}

	if (boundsB.m_type == MapModelBounds_Type_Box)
	{
		const Vector3 n1 = Vector3::ComputeNormal(boundsB.m_coords[0], boundsB.m_coords[1], boundsB.m_coords[3]);
		if (!__IsCollisionAlongNormal(n1, boundsA, boundsB))
			return false;
		const Vector3 n2 = Vector3::ComputeNormal(boundsB.m_coords[1], boundsB.m_coords[2], boundsB.m_coords[5]);
		if (!__IsCollisionAlongNormal(n1, boundsA, boundsB))
			return false;
		const Vector3 n3 = Vector3::ComputeNormal(boundsB.m_coords[0], boundsB.m_coords[4], boundsB.m_coords[1]);
		if (!__IsCollisionAlongNormal(n1, boundsA, boundsB))
			return false;
	}

	if ((boundsA.m_type == MapModelBounds_Type_Sphere)
		&& (boundsB.m_type == MapModelBounds_Type_Sphere))
	{
		const Vector3 AB = boundsB.m_center - boundsA.m_center;
		const Vector3 n = Vector3::Normalize(AB);
		if (!__IsCollisionAlongNormal(n, boundsA, boundsB))
			return false;
	}

	return true;
}

bool World::__IsCollisionAlongNormal(const Vector3& normal, const MapModelBounds& boundsA, const MapModelBounds& boundsB) const
{
	u32 cProjA = 0;
	u32 cProjB = 0;
	f32 projA[8];
	f32 projB[8];

	const Vector3 centerToCenter(boundsB.m_center.x - boundsA.m_center.x, boundsB.m_center.y - boundsA.m_center.y, boundsB.m_center.z - boundsA.m_center.z);
	const f32 projCenter = Vector3::Dot(centerToCenter, normal);

	if (boundsA.m_type == MapModelBounds_Type_Box)
	{
		assert(ARRAYSIZE(boundsA.m_coords) <= ARRAYSIZE(projA));
		for (u32 i = 0; i < ARRAYSIZE(boundsA.m_coords); ++i)
		{
			projA[i] = Vector3::Dot(boundsA.m_coords[i] - boundsA.m_center, normal);
		}
		cProjA = ARRAYSIZE(boundsA.m_coords);
		std::sort(projA, projA + cProjA);
	}
	else if (boundsA.m_type == MapModelBounds_Type_Sphere)
	{
		assert(2 <= ARRAYSIZE(projA));
		projA[0] = -boundsA.m_radius;
		projA[1] = +boundsA.m_radius;
		cProjA = 2;
	}

	if (boundsB.m_type == MapModelBounds_Type_Box)
	{
		assert(ARRAYSIZE(boundsB.m_coords) <= ARRAYSIZE(projB));
		for (u32 i = 0; i < ARRAYSIZE(boundsB.m_coords); ++i)
		{
			projB[i] = Vector3::Dot(boundsB.m_coords[i] - boundsB.m_center, normal);
		}
		cProjB = ARRAYSIZE(boundsB.m_coords);
		std::sort(projB, projB + cProjB);
	}
	else if (boundsB.m_type == MapModelBounds_Type_Sphere)
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

void World::__ParseMapStartElement(const u8* pszName, const u8** ppAttribs)
{
	if (_strcmpi(reinterpret_cast<const char*>(pszName), "model") == 0)
	{
		// models.
		u32 id = 0;
		const char* pszType = nullptr;
		const char* pszPath = nullptr;
		const char* pszModel = nullptr;
		RenderModel* pModel = nullptr;

		for (const u8** ppAttrib = ppAttribs; ppAttrib && *ppAttrib; ppAttrib += 2)
		{
			const char* pszAttribName = reinterpret_cast<const char*>(*(ppAttrib + 0));
			const char* pszValue = reinterpret_cast<const char*>(*(ppAttrib + 1));

			if (_strcmpi(pszAttribName, "id") == 0)
			{
				id = atol(pszValue);
			}
			else if (_strcmpi(pszAttribName, "type") == 0)
			{
				pszType = pszValue;
			}
			else if (_strcmpi(pszAttribName, "path") == 0)
			{
				pszPath = pszValue;
			}
			else if (_strcmpi(pszAttribName, "model") == 0)
			{
				pszModel = pszValue;
			}
		}

		if (id != 0 && pszType && pszPath)
		{
			if (_strcmpi(pszType, "texture") == 0)
			{
				std::string texturePath = m_mapPath;
				TB8::File::AppendToPath(texturePath, pszPath);
				pModel = RenderModel::AllocSquareFromTexture(__GetRenderer(), Vector3(0.f, 0.f, 1.f), Vector3(1.f, 0.f, 0.f), texturePath.c_str());
			}
			else if ((_strcmpi(pszType, "dae") == 0) && pszModel)
			{
				pModel = RenderModel::AllocFromDAE(__GetRenderer(), m_mapPath.c_str(), pszPath, pszModel);
			}
		}

		if (id != 0 && pModel)
		{
			m_mapModels.insert(std::make_pair(id, pModel));
		}
	}
	if (_strcmpi(reinterpret_cast<const char*>(pszName), "size") == 0)
	{
		u32 defaultTile = 0;

		for (const u8** ppAttrib = ppAttribs; ppAttrib && *ppAttrib; ppAttrib += 2)
		{
			const char* pszAttribName = reinterpret_cast<const char*>(*(ppAttrib + 0));
			const char* pszValue = reinterpret_cast<const char*>(*(ppAttrib + 1));

			if (_strcmpi(pszAttribName, "x") == 0)
			{
				m_mapSize.x = atol(pszValue);
			}
			else if (_strcmpi(pszAttribName, "y") == 0)
			{
				m_mapSize.y = atol(pszValue);
			}
			else if (_strcmpi(pszAttribName, "default_tile") == 0)
			{
				defaultTile = atol(pszValue);
			}
		}

		m_map.resize(m_mapSize.x * m_mapSize.y);

		IVector2 pos;
		for (pos.x = 0; pos.x < m_mapSize.x; ++pos.x)
		{
			for (pos.y = 0; pos.y < m_mapSize.y; ++pos.y)
			{
				MapCell& cell = __GetCell(pos);

				MapModel& model = cell.m_tile;
				std::map<u32, RenderModel*>::iterator itModel = m_mapModels.find(defaultTile);
				if (itModel != m_mapModels.end())
				{
					model.m_modelID = defaultTile;
					model.m_pModel = itModel->second;
					model.m_scale = 1.f;
					model.m_rotation = 0.f;
					model.m_pos = Vector3(static_cast<f32>(pos.x), static_cast<f32>(pos.y), 0.f);
					__ComputeModelWorldLocalTransform(model);
				}
			}
		}
	}
	if (_strcmpi(reinterpret_cast<const char*>(pszName), "start") == 0)
	{
		Vector3 posStart(0.f, 0.f, 0.f);
		for (const u8** ppAttrib = ppAttribs; ppAttrib && *ppAttrib; ppAttrib += 2)
		{
			const char* pszAttribName = reinterpret_cast<const char*>(*(ppAttrib + 0));
			const char* pszValue = reinterpret_cast<const char*>(*(ppAttrib + 1));

			if (_strcmpi(pszAttribName, "x") == 0)
			{
				posStart.x = static_cast<float>(atol(pszValue));
			}
			else if (_strcmpi(pszAttribName, "y") == 0)
			{
				posStart.y = static_cast<float>(atol(pszValue));
			}
		}

		m_characterModel.m_pos = posStart;
	}
	if (_strcmpi(reinterpret_cast<const char*>(pszName), "cell") == 0)
	{
		IVector2 pos;
		for (const u8** ppAttrib = ppAttribs; ppAttrib && *ppAttrib; ppAttrib += 2)
		{
			const char* pszAttribName = reinterpret_cast<const char*>(*(ppAttrib + 0));
			const char* pszValue = reinterpret_cast<const char*>(*(ppAttrib + 1));

			if (_strcmpi(pszAttribName, "pos") == 0)
			{
				std::vector<std::string> values;
				StrTok(pszValue, " ,", &values);
				if (values.size() == 2)
				{
					pos.x = atol(values[0].c_str());
					pos.y = atol(values[1].c_str());
				}
			}
			else if (_strcmpi(pszAttribName, "wall") == 0)
			{
				std::vector<std::string> values;
				StrTok(pszValue, " ,", &values);
				if (values.size() == 2)
				{
					u32 index = 0;
					f32 rotation = 0.f;
					Vector2 offset;
					if (_strcmpi(values[0].c_str(), "top") == 0)
					{
						index = 0;
						rotation = 0.f;
						offset = Vector2(0.5f, 0.f);
					}
					else if (_strcmpi(values[0].c_str(), "left") == 0)
					{
						index = 1;
						rotation = -90.f;
						offset = Vector2(0.0f, 0.5f);
					}
					else if (_strcmpi(values[0].c_str(), "right") == 0)
					{
						index = 2;
						rotation = +90.f;
						offset = Vector2(1.0f, 0.5f);
					}
					else if (_strcmpi(values[0].c_str(), "bottom") == 0)
					{
						index = 3;
						rotation = +180.f;
						offset = Vector2(0.5f, 1.0f);
					}

					MapCell& cell = m_map[(pos.y * m_mapSize.x) + pos.x];
					MapModel& model = cell.m_wall[index];
					const u32 modelID = atol(values[1].c_str());
					std::map<u32, RenderModel*>::iterator it = m_mapModels.find(modelID);
					if (it != m_mapModels.end())
					{
						RenderModel* pModel = it->second;
						const std::vector<RenderModel_Mesh>& meshes = it->second->GetMeshes();
						if (!meshes.empty())
						{
							const RenderModel_Mesh& mesh = meshes.front();

							model.m_modelID = modelID;
							model.m_pos = Vector3(static_cast<f32>(pos.x) + offset.x, static_cast<f32>(pos.y) + offset.y, 0.f);
							model.m_scale = 1.0f / mesh.m_size.x;
							model.m_rotation = rotation;
							model.m_pModel = pModel;

							__ComputeModelWorldLocalTransform(model);
							__ComputeCharacterModelBox(model, &(model.m_bounds));
						}
					}
				}
			}
		}
	}
}

void World::__ParseMapCharacters(const u8* value, int len)
{
}

void World::__ParseMapEndElement(const u8* name)
{
}

XML_Parser_Result World::__ParseMapRead(TB8::File* f, u8* pBuf, u32* pSize)
{
	*pSize = f->Read(pBuf, *pSize);
	return (*pSize > 0) ? XML_Parser_Result_Success : XML_Parser_Result_EOF;
}

 



}
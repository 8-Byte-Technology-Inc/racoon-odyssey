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
	, m_characterFacing(0.f)
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

	m_characterModel.m_pModel = pModel;

	const std::vector<RenderModel_Mesh>& meshes = m_characterModel.m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	MapModelBox& box = m_characterModel.m_volume;

	box.m_center = Vector3((mesh.m_max.x + mesh.m_min.x) / 2.f, (mesh.m_max.y + mesh.m_min.y) / 2.f, (mesh.m_max.z + mesh.m_min.z) / 2.f);
	box.m_coords[0] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_min.z);
	box.m_coords[1] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_min.z);
	box.m_coords[2] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_min.z);
	box.m_coords[3] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_min.z);
	box.m_coords[4] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_max.z);
	box.m_coords[5] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_max.z);
	box.m_coords[6] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_max.z);
	box.m_coords[7] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_max.z);

	// load default transform.
	m_characterModel.m_transform = __ComputeCharacterModelWorldTransform(m_characterPosition, m_characterFacing);
}

void World::Update(s32 frameCount)
{
	// compute environmental forces.
	const f32 forceFactor = 20.f;
	const f32 forceDampen = 10.0f;
	const f32 windResistFactor = 0.25f;
	const f32 gravityFactor = 5.0f;
	const f32 elapsedTime = static_cast<f32>(frameCount) / 60.f;
	const f32 velocityMax = 5.f; // m/s.

	Vector3 forceChar;
	forceChar.x = m_characterForce.x * forceFactor;
	forceChar.y = m_characterForce.y * forceFactor;
	forceChar.z = m_characterForce.z * forceFactor;

	const f32 velTotalSq = (m_characterVelocity.x * m_characterVelocity.x) + (m_characterVelocity.y * m_characterVelocity.y) + (m_characterVelocity.z * m_characterVelocity.z);

	Vector3 forceEnv;
	forceEnv.x = (forceDampen + (velTotalSq * windResistFactor)) * get_sign(m_characterVelocity.x) * -1.f;
	forceEnv.y = (forceDampen + (velTotalSq * windResistFactor)) * get_sign(m_characterVelocity.y) * -1.f;
	forceEnv.z = (m_characterMass * gravityFactor) * -1.f;

	// compute net force.
	Vector3 forceNet;
	forceNet.x = forceChar.x + forceEnv.x;
	forceNet.y = forceChar.y + forceEnv.y;
	forceNet.z = forceChar.z + forceEnv.z;

	// compute accel.
	Vector3 accel;
	accel.x = forceNet.x / m_characterMass;
	accel.y = forceNet.y / m_characterMass;
	accel.z = forceNet.z / m_characterMass;

	// compute new velocity.
	Vector3 vel;
	vel.x = m_characterVelocity.x + (accel.x * elapsedTime);
	vel.y = m_characterVelocity.y + (accel.y * elapsedTime);
	vel.z = m_characterVelocity.z + (accel.z * elapsedTime);

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

	Vector3 vel0 = m_characterVelocity;
	Vector3 vel1 = vel;

	// compute new position.
	Vector3 pos;
	pos.x = m_characterPosition.x + ((vel0.x + (0.5f * (vel1.x - vel0.x))) * elapsedTime);
	pos.y = m_characterPosition.y + ((vel0.y + (0.5f * (vel1.y - vel0.y))) * elapsedTime);
	pos.z = m_characterPosition.z + ((vel0.z + (0.5f * (vel1.z - vel0.z))) * elapsedTime);

	// compute new facing.
	f32 facing = m_characterFacing;
	if (!is_approx_zero(vel1.x) || !is_approx_zero(vel1.y))
	{
		Vector2 facingVector(vel1.x, vel1.y);
		facingVector.Normalize();
		const f32 angle = std::atan2(facingVector.y, facingVector.x);
		facing = (angle / DirectX::XM_PI) * 180.f;
	}

	// compute new transform.
	Matrix4 worldTransform = __ComputeCharacterModelWorldTransform(pos, facing);

	// compute new bounding box.
	MapModelBox box;
	{
		const std::vector<RenderModel_Mesh>& meshes = m_characterModel.m_pModel->GetMeshes();
		assert(!meshes.empty());
		const RenderModel_Mesh& mesh = meshes.front();

		box.m_center = Vector3((mesh.m_max.x + mesh.m_min.x) / 2.f, (mesh.m_max.y + mesh.m_min.y) / 2.f, (mesh.m_max.z + mesh.m_min.z) / 2.f);
		box.m_coords[0] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_min.z);
		box.m_coords[1] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_min.z);
		box.m_coords[2] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_min.z);
		box.m_coords[3] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_min.z);
		box.m_coords[4] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_max.z);
		box.m_coords[5] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_max.z);
		box.m_coords[6] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_max.z);
		box.m_coords[7] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_max.z);

		box.m_center = Matrix4::MultiplyVector(box.m_center, worldTransform);
		for (u32 i = 0; i < ARRAYSIZE(box.m_coords); ++i)
		{
			box.m_coords[i] = Matrix4::MultiplyVector(box.m_coords[i], worldTransform);
		}
	}

	// can't go lower than 0.
	if (pos.z < 0.f)
	{
		pos.z = 0.f;
		vel.z = 0.f;
	}

	// did we collide with a wall?
	{
		// compute which tiles to examine for collisions.
		IRect tiles;
		tiles.left = static_cast<u32>(std::min<f32>(pos.x, m_characterPosition.x) / TILES_PER_METER) - 1;
		tiles.right = static_cast<u32>(std::max<f32>(pos.x, m_characterPosition.x) / TILES_PER_METER) + 1;
		tiles.top = static_cast<u32>(std::min<f32>(pos.y, m_characterPosition.y) / TILES_PER_METER) - 1;
		tiles.bottom = static_cast<u32>(std::max<f32>(pos.y, m_characterPosition.y) / TILES_PER_METER) + 1;

		// look at walls for each tile.
		IVector2 cellPos;
		bool isCollision = false;
		Vector3 distMin(0.f, 0.f, 0.f);
		for (cellPos.x = tiles.left; cellPos.x < tiles.right; ++cellPos.x)
		{
			for (cellPos.y = tiles.top; cellPos.y < tiles.bottom; ++cellPos.y)
			{
				MapCell& cell = __GetCell(cellPos);
				for (u32 i = 0; i < ARRAYSIZE(cell.m_wall); ++i)
				{
					MapModel& wallModel = cell.m_wall[i];
					if (!wallModel.m_pModel)
						continue;
					Vector3 dist;
					if (!__IsCollision(box, wallModel.m_volume, &dist))
						continue;
					isCollision = true;
					if (dist.Mag() > distMin.Mag())
						distMin = dist;
				}
			}
		}
#if 0
		if (isCollision)
		{
			const Vector3 vectorReverse = m_characterPosition - pos;
			const f32 reverseMag = vectorReverse.Mag();
			const Vector3 vectorReverseNormalized = vectorReverse / reverseMag;
			const f32 projectedReverse = Vector3::Dot(distMin, vectorReverseNormalized);
			const f32 ratio = projectedReverse / reverseMag;
			const Vector3 actualReverse = vectorReverse * ratio;;

			pos = pos + actualReverse;
			vel.x = 0;
			vel.y = 0;
			vel.z = 0;

			worldTransform = __ComputeCharacterModelWorldTransform(pos, facing);
		}
#endif
	}

	// update.
	m_characterPosition = pos;
	m_characterVelocity = vel;
	m_characterFacing = facing;
	m_characterModel.m_transform = worldTransform;
	//m_characterModel.m_volume = box;
}

void World::Render(RenderMain* pRenderer)
{

	// draw all the tiles that might be on-screen.
	const Vector2& screenSizeWorld = pRenderer->GetRenderScreenSizeWorld();

	// compute which tiles we'll draw.
	IRect tiles;
	tiles.left = static_cast<u32>((m_characterPosition.x - ((screenSizeWorld.x / 2.f) * 3.f) / TILES_PER_METER));
	tiles.right = static_cast<u32>((m_characterPosition.x + ((screenSizeWorld.x / 2.f) * 3.f) / TILES_PER_METER)) + 1;
	tiles.top = static_cast<u32>((m_characterPosition.y - ((screenSizeWorld.y / 2.f) * 3.f) / TILES_PER_METER));
	tiles.bottom = static_cast<u32>((m_characterPosition.y + ((screenSizeWorld.y / 2.f) * 3.f) / TILES_PER_METER)) + 1;

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
					model.m_pModel->SetWorldTransform(model.m_transform);
					model.m_pModel->Render(pRenderer);
				}
			}

			for (u32 i = 0; i < ARRAYSIZE(cell.m_wall); ++i)
			{
				MapModel& model = cell.m_wall[i];
				if (model.m_pModel)
				{
					model.m_pModel->SetWorldTransform(model.m_transform);
					model.m_pModel->Render(pRenderer);
				}
			}
		}
	}

	// draw the character.
	{
		m_characterModel.m_pModel->SetWorldTransform(m_characterModel.m_transform);
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
		break;
	}
	default:
		break;
	}
}

Matrix4 World::__ComputeCharacterModelWorldTransform(const Vector3& pos, f32 facing)
{
	const std::vector<RenderModel_Mesh>& meshes = m_characterModel.m_pModel->GetMeshes();
	assert(!meshes.empty());
	const RenderModel_Mesh& mesh = meshes.front();

	// center it, still in directX coords.
	Matrix4 matrixCenter;
	matrixCenter.SetTranslation(Vector3(0.f - m_characterModel.m_volume.m_center.x, 0.f - mesh.m_min.y, 0.f - m_characterModel.m_volume.m_center.z));

	// scale the model so <y> is 1/2 meter.
	const f32 scale = .75f / mesh.m_size.y;
	Matrix4 matrixScale;
	matrixScale.SetScale(Vector3(scale, scale, scale));

	// translate from directX to world coordinate system.
	Matrix4 matrixCoordsToWorld;
	matrixCoordsToWorld.SetIdentity();
	matrixCoordsToWorld.m[1][1] = 0.0f;
	matrixCoordsToWorld.m[2][1] = -1.0f;
	matrixCoordsToWorld.m[2][2] = 0.0f;
	matrixCoordsToWorld.m[1][2] = 1.0f;

	// rotate it.
	Matrix4 matrixRotate;
	matrixRotate.SetRotate(Vector3(0.f, 0.f, DirectX::XM_PI * (facing) / 180.f));

	// position it.
	Matrix4 matrixPosition;
	matrixPosition.SetTranslation(pos);

	// compute the overall transform.
	Matrix4 worldTransform = matrixPosition;
	worldTransform = Matrix4::MultiplyAB(worldTransform, matrixRotate);
	worldTransform = Matrix4::MultiplyAB(worldTransform, matrixCoordsToWorld);
	worldTransform = Matrix4::MultiplyAB(worldTransform, matrixScale);
	worldTransform = Matrix4::MultiplyAB(worldTransform, matrixCenter);

	return worldTransform;
}

bool World::__IsCollision(const MapModelBox& boxA, const MapModelBox& boxB, Vector3* pDist)
{
	f32 distA = 100.f;
	if (!__IsCollision(Vector3(1.0, 0.0, 0.0), boxA, boxB, &distA))
		return false;
	f32 distB = 100.f;
	if (!__IsCollision(Vector3(0.0, 1.0, 0.0), boxA, boxB, &distB))
		return false;
	f32 distC = 100.f;
	if (!__IsCollision(Vector3(0.0, 0.0, 1.0), boxA, boxB, &distC))
		return false;

	if (distA > distB && distA > distC)
	{
		*pDist = Vector3(distA, 0.f, 0.f);
	}
	else if (distB > distA&& distB > distC)
	{
		*pDist = Vector3(0.f, distB, 0.f);
	}
	else
	{
		*pDist = Vector3(0.f, 0.f, distC);
	}
	return true;
}

bool World::__IsCollision(const Vector3& projection, const MapModelBox& boxA, const MapModelBox& boxB, f32* pDist)
{
	const Vector3 centerToCenter(boxB.m_center.x - boxA.m_center.x, boxB.m_center.y - boxA.m_center.y, boxB.m_center.z - boxA.m_center.z);
	const f32 projCenter = Vector3::Dot(centerToCenter, projection);
	std::vector<f32> projA;
	projA.resize(8);
	std::vector<f32> projB;
	projB.resize(8);
	for (u32 i = 0; i < 8; ++i)
	{
		projA[i] = Vector3::Dot(boxA.m_coords[i] - boxA.m_center, projection);
		projB[i] = Vector3::Dot(boxB.m_coords[i] - boxB.m_center, projection);
	}

	std::sort(projA.begin(), projA.end());
	std::sort(projB.begin(), projB.end());

	if (projCenter < 0.f)
	{
		*pDist = (-1.f * projCenter) - projB.back() + projA.front();
	}
	else
	{
		*pDist = projCenter - projA.back() + projB.front();
	}
	return (*pDist < 0.f);
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
				pModel = RenderModel::AllocSquareFromTexture(__GetRenderer(), Vector2(1.f, 1.f), texturePath.c_str());
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
				model.m_modelID = defaultTile;
				std::map<u32, RenderModel*>::iterator itModel = m_mapModels.find(model.m_modelID);
				if (itModel != m_mapModels.end())
				{
					model.m_pModel = itModel->second;

					// position it.
					Matrix4 matrixPosition;
					matrixPosition.SetTranslation(Vector3(static_cast<f32>(pos.x), static_cast<f32>(pos.y), 0.f));

					model.m_transform = matrixPosition;
				}
			}
		}
	}
	if (_strcmpi(reinterpret_cast<const char*>(pszName), "start") == 0)
	{
		for (const u8** ppAttrib = ppAttribs; ppAttrib && *ppAttrib; ppAttrib += 2)
		{
			const char* pszAttribName = reinterpret_cast<const char*>(*(ppAttrib + 0));
			const char* pszValue = reinterpret_cast<const char*>(*(ppAttrib + 1));

			if (_strcmpi(pszAttribName, "x") == 0)
			{
				m_characterPosition.x = static_cast<float>(atol(pszValue));
			}
			else if (_strcmpi(pszAttribName, "y") == 0)
			{
				m_characterPosition.y = static_cast<float>(atol(pszValue));
			}
		}
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
						const std::vector<RenderModel_Mesh>& meshes = it->second->GetMeshes();
						if (!meshes.empty())
						{
							const RenderModel_Mesh& mesh = meshes.front();

							model.m_volume.m_center = Vector3((mesh.m_max.x + mesh.m_min.x) / 2.f, (mesh.m_max.y + mesh.m_min.y) / 2.f, (mesh.m_max.z + mesh.m_min.z) / 2.f);
							model.m_volume.m_coords[0] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_min.z);
							model.m_volume.m_coords[1] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_min.z);
							model.m_volume.m_coords[2] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_min.z);
							model.m_volume.m_coords[3] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_min.z);
							model.m_volume.m_coords[4] = Vector3(mesh.m_min.x, mesh.m_min.y, mesh.m_max.z);
							model.m_volume.m_coords[5] = Vector3(mesh.m_max.x, mesh.m_min.y, mesh.m_max.z);
							model.m_volume.m_coords[6] = Vector3(mesh.m_max.x, mesh.m_max.y, mesh.m_max.z);
							model.m_volume.m_coords[7] = Vector3(mesh.m_min.x, mesh.m_max.y, mesh.m_max.z);

							// center it, still in directX coords.
							Matrix4 matrixCenter;
							matrixCenter.SetTranslation(Vector3(0.f - model.m_volume.m_center.x, 0.f - mesh.m_min.y, 0.f - model.m_volume.m_center.z));

							// scale the model so <x> is one meter.
							const f32 scale = 1.0f / mesh.m_size.x;
							Matrix4 matrixScale;
							matrixScale.SetScale(Vector3(scale, scale, scale));

							// translate from directX to world coordinate system.
							Matrix4 matrixCoordsToWorld;
							matrixCoordsToWorld.SetIdentity();
							matrixCoordsToWorld.m[1][1] = 0.0f;
							matrixCoordsToWorld.m[2][1] = -1.0f;
							matrixCoordsToWorld.m[2][2] = 0.0f;
							matrixCoordsToWorld.m[1][2] = 1.0f;

							// rotate it.
							Matrix4 matrixRotate;
							matrixRotate.SetRotate(Vector3(0.f, 0.f, (DirectX::XM_PI * rotation) / 180.f));

							// position it.
							Matrix4 matrixPosition;
							matrixPosition.SetTranslation(Vector3(static_cast<f32>(pos.x) + offset.x, static_cast<f32>(pos.y) + offset.y, 0.f));

							// compute the overall transform.
							Matrix4 worldTransform = matrixPosition;
							worldTransform = Matrix4::MultiplyAB(worldTransform, matrixRotate);
							worldTransform = Matrix4::MultiplyAB(worldTransform, matrixCoordsToWorld);
							worldTransform = Matrix4::MultiplyAB(worldTransform, matrixScale);
							worldTransform = Matrix4::MultiplyAB(worldTransform, matrixCenter);

							// test.
							const Vector3 test_in(2.f, 0.5f, -.25f);

							const Vector3 test_out = Matrix4::MultiplyVector(test_in, worldTransform);

							const Vector3 test1 = Matrix4::MultiplyVector(test_in, matrixCenter);
							const Vector3 test2 = Matrix4::MultiplyVector(test1, matrixScale);
							const Vector3 test3 = Matrix4::MultiplyVector(test2, matrixCoordsToWorld);
							const Vector3 test4 = Matrix4::MultiplyVector(test3, matrixRotate);
							const Vector3 test5 = Matrix4::MultiplyVector(test4, matrixPosition);

							model.m_modelID = modelID;
							model.m_pModel = it->second;
							model.m_transform = worldTransform;

							model.m_volume.m_center = Matrix4::MultiplyVector(model.m_volume.m_center, worldTransform);
							for (u32 i = 0; i < ARRAYSIZE(model.m_volume.m_coords); ++i)
							{
								model.m_volume.m_coords[i] = Matrix4::MultiplyVector(model.m_volume.m_coords[i], worldTransform);
							}
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
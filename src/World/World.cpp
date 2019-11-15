#include "pch.h"

#include "common/memory.h"
#include "common/parse_xml.h"
#include "common/string.h"

#include "event/EventQueue.h"
#include "event/EventQueueMessages.h"

#include "render/RenderMain.h"
#include "render/RenderModel.h"
#include "render/RenderImagine.h"
#include "render/RenderTexture.h"
#include "render/RenderStatusBars.h"

#include "Object.h"
#include "Unit.h"
#include "Avatar.h"
#include "World.h"

namespace TB8
{

const f32 TILES_PER_METER = 1.0f;

World::World(Client_Globals* pGlobalState)
	: Client_Globals_Accessor(pGlobalState)
	, m_pCharacterObj(nullptr)
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
	const RenderModel_Bounds& meshBounds = mesh.m_bounds;

	m_pCharacterObj = World_Avatar::Alloc(__GetGlobals(), pszCharacterModelPath);

	m_pCharacterObj->m_modelID = 0;
	m_pCharacterObj->m_type = World_Object_Type_Avatar;
	m_pCharacterObj->m_pModel = pModel;
	// scale the model so <y> is 0.75 meters.
	m_pCharacterObj->m_pos = m_startPos;
	m_pCharacterObj->m_scale = .75f / meshBounds.m_size.y;
	m_pCharacterObj->m_mass = 2.f;
	m_pCharacterObj->m_bounds.m_type = World_Object_Bounds_Type_Sphere;

	m_pCharacterObj->Init();
}

void World::Update(s32 frameCount)
{
	Vector3 pos;
	Vector3 vel;
	m_pCharacterObj->ComputeNextPosition(frameCount, pos, vel);

	// adjust position & velocity for collisions.
	__AdjustCharacterModelPositionForCollisions(pos, vel);

	// update.
	m_pCharacterObj->UpdatePosition(frameCount, pos, vel);

	m_pCharacterObj->Update(frameCount);
}

void World::Render3D(RenderMain* pRenderer)
{
	// draw all the tiles that might be on-screen.
	const Vector2& screenSizeWorld = pRenderer->GetRenderScreenSizeWorld();

	// compute which tiles we'll draw.
	IRect tiles;
	tiles.left = static_cast<u32>((m_pCharacterObj->m_pos.x - ((screenSizeWorld.x / 2.f) * 3.f) / TILES_PER_METER));
	tiles.right = static_cast<u32>((m_pCharacterObj->m_pos.x + ((screenSizeWorld.x / 2.f) * 3.f) / TILES_PER_METER)) + 1;
	tiles.top = static_cast<u32>((m_pCharacterObj->m_pos.y - ((screenSizeWorld.y / 2.f) * 4.f) / TILES_PER_METER));
	tiles.bottom = static_cast<u32>((m_pCharacterObj->m_pos.y + ((screenSizeWorld.y / 2.f) * 4.f) / TILES_PER_METER)) + 1;

	Vector3 screenWorldPos = m_pCharacterObj->m_pos;
	__GetRenderer()->AlignWorldPosition(screenWorldPos);

	// draw the tiles & walls.
	IVector2 cellPos;
	for (cellPos.y = tiles.top; cellPos.y < tiles.bottom; ++cellPos.y)
	{
		cellPos.x = tiles.left;
		for (std::multimap<IVector2, World_Object*>::iterator it = m_objects.lower_bound(cellPos);  
			(it != m_objects.end()) && (it->first.y == cellPos.y) && (it->first.x <= tiles.right); ++it)
		{
			World_Object* pObj = it->second;
			pObj->Render3D(screenWorldPos);
		}
	}

	// draw the character.
	{
		m_pCharacterObj->Render3D(m_pCharacterObj->m_pos);
	}
}

void World::Render2D(RenderMain* pRenderer)
{
	m_pCharacterObj->Render2D(m_pCharacterObj->m_pos);
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

	for (std::multimap<IVector2, World_Object*>::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		OBJFREE(it->second);
	}

	OBJFREE(m_pCharacterObj);

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
			const Vector3 force(static_cast<f32>(pEventMoveStart->m_dx), m_pCharacterObj->GetForce().y, m_pCharacterObj->GetForce().z);
			m_pCharacterObj->SetForce(force);
		}
		if (pEventMoveStart->m_dy != 0)
		{
			const Vector3 force(m_pCharacterObj->GetForce().x, static_cast<f32>(pEventMoveStart->m_dy), m_pCharacterObj->GetForce().z);
			m_pCharacterObj->SetForce(force);
		}
		if (pEventMoveStart->m_dz != 0)
		{
			const Vector3 force(m_pCharacterObj->GetForce().x, m_pCharacterObj->GetForce().y, static_cast<f32>(pEventMoveStart->m_dz));
			m_pCharacterObj->SetForce(force);
		}
		break;
	}
	case EventMessageID_MoveEnd:
	{
		EventMessage_MoveStart* pEventMoveEnd = static_cast<EventMessage_MoveStart*>(pEvent);
		if (pEventMoveEnd->m_dx != 0)
		{
			const Vector3 force(0.f, m_pCharacterObj->GetForce().y, m_pCharacterObj->GetForce().z);
			m_pCharacterObj->SetForce(force);
		}
		if (pEventMoveEnd->m_dy != 0)
		{
			const Vector3 force(m_pCharacterObj->GetForce().x, 0.f, m_pCharacterObj->GetForce().z);
			m_pCharacterObj->SetForce(force);
		}
		if (pEventMoveEnd->m_dz != 0)
		{
			const Vector3 force(m_pCharacterObj->GetForce().x, m_pCharacterObj->GetForce().y, 0.f);
			m_pCharacterObj->SetForce(force);
		}
		break;
	}
	default:
		break;
	}
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
	const World_Object& object = *m_pCharacterObj;
	const Vector3 posDelta = pos - object.m_pos;
	const f32 posDeltaAxisMag = Vector3::Dot(posDelta, axis);
	if (is_approx_zero(posDeltaAxisMag))
		return;
	if (!__IsCharacterModelCollideWithWall(object.m_pos + (axis * posDeltaAxisMag)))
		return;

	// this axis collides. interpolate position.
	f32 min = 0.f;
	f32 max = posDeltaAxisMag;

	for (u32 i = 0; i < 8; ++i)
	{
		const f32 mid = (max + min) / 2.f;
		if (__IsCharacterModelCollideWithWall(object.m_pos + (axis * mid)))
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
		pos.x = object.m_pos.x + min;
		vel.x = 0.f;
	}
	if (!is_approx_zero(axis.y))
	{
		pos.y = object.m_pos.y + min;
		vel.y = 0.f;
	}
	if (!is_approx_zero(axis.z))
	{
		pos.z = object.m_pos.z + min;
		vel.z = 0.f;
	}
}

bool World::__IsCharacterModelCollideWithWall(const Vector3& posNew) const
{
	const World_Object& object = *m_pCharacterObj;
	const Vector3 posDelta = posNew - object.m_pos;

	World_Object_Bounds boundsNew = object.m_bounds;
	boundsNew.m_center = boundsNew.m_center + posDelta;

	// compute which tiles to examine for collisions.
	IRect tiles;
	tiles.left = static_cast<u32>(posNew.x / TILES_PER_METER) - 1;
	tiles.right = static_cast<u32>(posNew.x / TILES_PER_METER) + 1;
	tiles.top = static_cast<u32>(posNew.y / TILES_PER_METER) - 1;
	tiles.bottom = static_cast<u32>(posNew.y / TILES_PER_METER) + 1;

	// look at walls for each tile.
	IVector2 cellPos;
	for (cellPos.y = tiles.top; cellPos.y <= tiles.bottom; ++cellPos.y)
	{
		cellPos.x = tiles.left;
		for (std::multimap<IVector2, World_Object*>::const_iterator it = m_objects.lower_bound(cellPos);
			(it != m_objects.end()) && (it->first.y == cellPos.y) && (it->first.x <= tiles.right); ++it)
		{
			World_Object* pObj = it->second;
			if (!pObj->IsCollision(boundsNew))
				continue;
			return true;
		}
	}

	return false;
}

void World::__ParseMapStartElement(const char* pszName, const char** ppAttribs)
{
	if (_strcmpi(reinterpret_cast<const char*>(pszName), "model") == 0)
	{
		// models.
		u32 id = 0;
		const char* pszType = nullptr;
		const char* pszPath = nullptr;
		const char* pszModel = nullptr;
		RenderModel* pModel = nullptr;

		for (const char** ppAttrib = ppAttribs; ppAttrib && *ppAttrib; ppAttrib += 2)
		{
			const char* pszAttribName = *(ppAttrib + 0);
			const char* pszValue = *(ppAttrib + 1);

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
				RenderTexture* pTexture = RenderTexture::Alloc(__GetRenderer(), texturePath.c_str());
				pModel = RenderModel::AllocSimpleRectangle(__GetRenderer(), RenderMainViewType_World, Vector3(0.f, 0.f, 1.f), Vector3(1.f, 0.f, 0.f), 
															pTexture, Vector2(0.f, 0.f), Vector2(1.f, 1.f));
				RELEASEI(pTexture);
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

		for (const char** ppAttrib = ppAttribs; ppAttrib && *ppAttrib; ppAttrib += 2)
		{
			const char* pszAttribName = *(ppAttrib + 0);
			const char* pszValue = *(ppAttrib + 1);

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

		IVector2 pos;
		for (pos.x = 0; pos.x < m_mapSize.x; ++pos.x)
		{
			for (pos.y = 0; pos.y < m_mapSize.y; ++pos.y)
			{
				std::map<u32, RenderModel*>::iterator itModel = m_mapModels.find(defaultTile);
				if (itModel != m_mapModels.end())
				{
					World_Object* pObj = World_Object::Alloc(__GetGlobals());
					pObj->m_modelID = defaultTile;
					pObj->m_type = World_Object_Type_Tile;
					pObj->m_pModel = itModel->second;
					pObj->m_scale = 1.f;
					pObj->m_rotation = 0.f;
					pObj->m_pos = Vector3(static_cast<f32>(pos.x), static_cast<f32>(pos.y), 0.f);
					pObj->Init();

					m_objects.insert(std::make_pair(pos, pObj));
				}
			}
		}
	}
	if (_strcmpi(reinterpret_cast<const char*>(pszName), "start") == 0)
	{
		Vector3 posStart(0.f, 0.f, 0.f);
		for (const char** ppAttrib = ppAttribs; ppAttrib && *ppAttrib; ppAttrib += 2)
		{
			const char* pszAttribName = *(ppAttrib + 0);
			const char* pszValue = *(ppAttrib + 1);

			if (_strcmpi(pszAttribName, "x") == 0)
			{
				posStart.x = static_cast<float>(atol(pszValue));
			}
			else if (_strcmpi(pszAttribName, "y") == 0)
			{
				posStart.y = static_cast<float>(atol(pszValue));
			}
		}

		m_startPos = posStart;
	}
	if (_strcmpi(reinterpret_cast<const char*>(pszName), "cell") == 0)
	{
		IVector2 pos;
		for (const char** ppAttrib = ppAttribs; ppAttrib && *ppAttrib; ppAttrib += 2)
		{
			const char* pszAttribName = *(ppAttrib + 0);
			const char* pszValue = *(ppAttrib + 1);

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

					const u32 modelID = atol(values[1].c_str());
					std::map<u32, RenderModel*>::iterator it = m_mapModels.find(modelID);
					if (it != m_mapModels.end())
					{
						RenderModel* pModel = it->second;
						const std::vector<RenderModel_Mesh>& meshes = it->second->GetMeshes();
						if (!meshes.empty())
						{
							World_Object* pObj = World_Object::Alloc(__GetGlobals());

							const RenderModel_Mesh& mesh = meshes.front();
							const RenderModel_Bounds& meshBounds = mesh.m_bounds;

							pObj->m_modelID = modelID;
							pObj->m_type = World_Object_Type_Wall;
							pObj->m_pos = Vector3(static_cast<f32>(pos.x) + offset.x, static_cast<f32>(pos.y) + offset.y, 0.f);
							pObj->m_scale = 1.0f / meshBounds.m_size.x;
							pObj->m_rotation = rotation;
							pObj->m_pModel = pModel;
							pObj->m_bounds.m_type = World_Object_Bounds_Type_Box;

							pObj->Init();

							m_objects.insert(std::make_pair(IVector2(static_cast<s32>(pObj->m_pos.x), static_cast<s32>(pObj->m_pos.y)), pObj));
						}
					}
				}
			}
		}
	}
}

void World::__ParseMapCharacters(const char* value, int len)
{
}

void World::__ParseMapEndElement(const char* name)
{
}

XML_Parser_Result World::__ParseMapRead(TB8::File* f, u8* pBuf, u32* pSize)
{
	*pSize = f->Read(pBuf, *pSize);
	return (*pSize > 0) ? XML_Parser_Result_Success : XML_Parser_Result_EOF;
}

}
#pragma once

#include <string>
#include <vector>

#include "common/basic_types.h"

#include "render/RenderModel.h"

#include "client/Client_Globals.h"

namespace TB8
{

enum MapModelBounds_Type : u32
{
	MapModelBounds_Type_None,
	MapModelBounds_Type_Box,
	MapModelBounds_Type_Sphere,
};

struct MapModelBounds
{
	MapModelBounds()
		: m_type(MapModelBounds_Type_None)
		, m_radius(0.f)
	{
	}

	MapModelBounds_Type				m_type;
	Vector3							m_center;
	Vector3							m_coords[8];
	f32								m_radius;
};

struct MapModel
{
	MapModel()
		: m_modelID(0)
		, m_pModel(nullptr)
		, m_pos()
		, m_rotation(0.f)
		, m_scale(0.f)
		, m_worldLocalTransform()
	{
	}

	u32								m_modelID;
	RenderModel*					m_pModel;
	Vector3							m_pos;
	f32								m_rotation;
	f32								m_scale;
	Matrix4							m_worldLocalTransform;
	MapModelBounds					m_bounds;
};

struct MapCell
{
	MapModel		m_tile;
	MapModel		m_wall[4];
};

class World : public Client_Globals_Accessor
{
public:
	World(Client_Globals* pGlobalState);
	~World();

	static World* Alloc(Client_Globals* pGlobalState);
	void Free();

	void LoadMap(const char* pszMapPath);
	void LoadCharacter(const char* pszCharacterModelPath, const char* pszModelName);

	void Update(s32 frameCount);
	void Render(RenderMain* pRenderer);

protected:
	void __Initialize();
	void __Uninitialize();

	void __EventHandler(EventMessage* pEvent);

	void __ComputeModelWorldLocalTransform(MapModel& model);
	void __ComputeModelWorldTransformRender(MapModel& model, const Vector3& screenWorldPos, Matrix4* pWorldTransform);
	void __ComputeCharacterModelBox(MapModel& model, MapModelBounds* pBounds);
	void __ComputeCharacterModelSphere(MapModel& model, MapModelBounds* pBounds);

	void __AdjustCharacterModelPositionForCollisions(Vector3& pos, Vector3& vel);
	void __AdjustCharacterModelPositionForCollisionsAxis(Vector3& pos, Vector3& vel, const Vector3& axis);
	bool __IsCharacterModelCollideWithWall(const Vector3& posNew) const;
	bool __IsCollision(const MapModelBounds& boundsA, const MapModelBounds& boundsB) const;
	bool __IsCollisionAlongNormal(const Vector3& projection, const MapModelBounds& boundsA, const MapModelBounds& boundsB) const;

	void __ParseMapStartElement(const u8* name, const u8** atts);
	void __ParseMapCharacters(const u8* value, int len);
	void __ParseMapEndElement(const u8* name);
	static XML_Parser_Result __ParseMapRead(TB8::File* f, u8* pBuf, u32* pSize);

	MapCell& __GetCell(const IVector2& pos) { return m_map[(pos.y * m_mapSize.x) + pos.x]; }
	const MapCell& __GetCell(const IVector2& pos) const { return m_map[(pos.y * m_mapSize.x) + pos.x]; }

	std::string						m_mapPath;
	IVector2						m_mapSize;
	std::vector<MapCell>			m_map;
	std::map<u32, RenderModel*>		m_mapModels;

	MapModel						m_characterModel;

	f32								m_characterMass;
	Vector3							m_characterForce;
	Vector3							m_characterVelocity;
};


}
#pragma once

#include <string>
#include <vector>

#include "common/basic_types.h"

#include "render/RenderModel.h"

#include "client/Client_Globals.h"

namespace TB8
{

struct MapModelBox
{
	Vector3							m_center;
	Vector3							m_coords[8];
};

struct MapModel
{
	MapModel()
		: m_modelID(0)
		, m_pModel(nullptr)
		, m_transform()
	{
	}

	u32								m_modelID;
	RenderModel*					m_pModel;
	Matrix4							m_transform;
	MapModelBox						m_volume;
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

	const Vector3& GetCharacterPosition() const { return m_characterPosition; }

	void Update(s32 frameCount);
	void Render(RenderMain* pRenderer);

protected:
	void __Initialize();
	void __Uninitialize();

	void __EventHandler(EventMessage* pEvent);

	Matrix4 __ComputeCharacterModelWorldTransform(const Vector3& pos, f32 rotation);

	bool __IsCollision(const MapModelBox& boxA, const MapModelBox& boxB, Vector3* pDist);
	bool __IsCollision(const Vector3& vector, const MapModelBox& boxA, const MapModelBox& boxB, f32* pDist);

	void __ParseMapStartElement(const u8* name, const u8** atts);
	void __ParseMapCharacters(const u8* value, int len);
	void __ParseMapEndElement(const u8* name);
	static XML_Parser_Result __ParseMapRead(TB8::File* f, u8* pBuf, u32* pSize);

	MapCell& __GetCell(const IVector2& pos) { return m_map[(pos.y * m_mapSize.x) + pos.x]; }

	std::string						m_mapPath;
	IVector2						m_mapSize;
	std::vector<MapCell>			m_map;
	std::map<u32, RenderModel*>		m_mapModels;

	MapModel						m_characterModel;

	f32								m_characterMass;
	f32								m_characterFacing;
	Vector3							m_characterPosition;
	Vector3							m_characterForce;
	Vector3							m_characterVelocity;
};


}
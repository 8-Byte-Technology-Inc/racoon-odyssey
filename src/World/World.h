#pragma once

#include <string>
#include <map>

#include "common/basic_types.h"

#include "client/Client_Globals.h"

namespace TB8
{

struct World_Object;
struct World_Unit;
struct World_Avatar;
class RenderModel;
class RenderImagine;
class RenderStatusBars;

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
	void Render3D(RenderMain* pRenderer);
	void Render2D(RenderMain* pRenderer);

protected:
	void __Initialize();
	void __Uninitialize();

	void __EventHandler(EventMessage* pEvent);

	void __AdjustCharacterModelPositionForCollisions(Vector3& pos, Vector3& vel);
	void __AdjustCharacterModelPositionForCollisionsAxis(Vector3& pos, Vector3& vel, const Vector3& axis);
	bool __IsCharacterModelCollideWithWall(const Vector3& posNew) const;

	void __ParseMapStartElement(const char* name, const char** atts);
	void __ParseMapCharacters(const char* value, int len);
	void __ParseMapEndElement(const char* name);
	static XML_Parser_Result __ParseMapRead(TB8::File* f, u8* pBuf, u32* pSize);

	std::string									m_mapPath;
	IVector2									m_mapSize;
	Vector3										m_startPos;
	std::map<u32, RenderModel*>					m_mapModels;

	std::multimap<IVector2, World_Object*>		m_objects;

	World_Avatar*								m_pCharacterObj;
};


}
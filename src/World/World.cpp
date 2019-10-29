#include "pch.h"

#include "common/memory.h"
#include "common/parse_xml.h"
#include "common/string.h"

#include "render/RenderMain.h"
#include "render/RenderModel.h"

#include "World.h"

namespace TB8
{

World::World(Client_Globals* pGlobalState)
	: Client_Globals_Accessor(pGlobalState)
	, m_pCharacterModel(nullptr)
	, m_characterFacing(0.f)
{
}

World::~World()
{
	RELEASEI(m_pCharacterModel);
	for (std::map<u32, RenderModel*>::iterator it = m_mapModels.begin(); it != m_mapModels.end(); ++it)
	{
		RELEASEI(it->second);
	}
}

World* World::Alloc(Client_Globals* pGlobalState)
{
	return TB8_NEW(World)(pGlobalState);
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
	m_pCharacterModel = RenderModel::AllocFromDAE(__GetRenderer(), __GetPathAssets().c_str(), pszCharacterModelPath, pszModelName);
}

void World::Render(RenderMain* pRenderer)
{
	const f32 TILES_PER_METER = 1.0f;

	// draw all the tiles that might be on-screen.
	const Vector2& screenSizeWorld = pRenderer->GetRenderScreenSizeWorld();

	// compute which tiles we'll draw.
	IRect tiles;
	tiles.left = static_cast<u32>((m_characterPosition.x - ((screenSizeWorld.x / 2.f) * 2.5f) / TILES_PER_METER));
	tiles.right = static_cast<u32>((m_characterPosition.x + ((screenSizeWorld.x / 2.f) * 2.5f) / TILES_PER_METER)) + 1;
	tiles.top = static_cast<u32>((m_characterPosition.y - ((screenSizeWorld.y / 2.f) * 3.0f) / TILES_PER_METER));
	tiles.bottom = static_cast<u32>((m_characterPosition.y + ((screenSizeWorld.y / 2.f) * 3.0f) / TILES_PER_METER)) + 1;

	// draw the tiles.
	IVector2 cellPos;
	for (cellPos.x = tiles.left; cellPos.x < tiles.right; ++cellPos.x)
	{
		for (cellPos.y = tiles.top; cellPos.y < tiles.bottom; ++cellPos.y)
		{
			MapCell& cell = __GetCell(cellPos);
			std::map<u32, RenderModel*>::iterator it = m_mapModels.find(cell.m_tileID);
			assert(it != m_mapModels.end());
			RenderModel* pModel = it->second;
			pModel->SetPosition(Vector3(static_cast<f32>(cellPos.x) + 0.f, static_cast<f32>(cellPos.y) + 0.f, 0.f));
			pModel->Render(pRenderer);
		}
	}

	// draw the walls.
	for (cellPos.x = tiles.left; cellPos.x < tiles.right; ++cellPos.x)
	{
		for (cellPos.y = tiles.top; cellPos.y < tiles.bottom; ++cellPos.y)
		{
			MapCell& cell = __GetCell(cellPos);
			if (cell.m_wallID[0] != 0)
			{
				// top.
				std::map<u32, RenderModel*>::iterator it = m_mapModels.find(cell.m_wallID[0]);
				assert(it != m_mapModels.end());
				RenderModel* pModel = it->second;

				const std::vector<RenderModel_Mesh>& meshes = pModel->GetMeshes();
				assert(!meshes.empty());
				const RenderModel_Mesh& mesh = meshes.front();

				// center it, still in directX coords.
				Matrix4 matrixCenter;
				matrixCenter.SetTranslation(Vector3(0.f - ((mesh.m_max.x + mesh.m_min.x) / 2.f), 0.f - mesh.m_min.y, 0.f - ((mesh.m_max.z + mesh.m_min.z) / 2.f)));

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

				// position it.
				Matrix4 matrixPosition;
				matrixPosition.SetTranslation(Vector3(static_cast<f32>(cellPos.x) + 0.5f, static_cast<f32>(cellPos.y), 0.f));

				// compute the overall transform.
				Matrix4 worldTransform = matrixPosition;
				worldTransform = Matrix4::MultiplyAB(worldTransform, matrixCoordsToWorld);
				worldTransform = Matrix4::MultiplyAB(worldTransform, matrixScale);
				worldTransform = Matrix4::MultiplyAB(worldTransform, matrixCenter);

				// test.
				const Vector3 test_in(2.f, 0.5f, -.25f);

				const Vector3 test_out = Matrix4::MultiplyVector(test_in, worldTransform);

				const Vector3 test1 = Matrix4::MultiplyVector(test_in, matrixCenter);
				const Vector3 test2 = Matrix4::MultiplyVector(test1, matrixScale);
				const Vector3 test3 = Matrix4::MultiplyVector(test2, matrixCoordsToWorld);
				const Vector3 test4 = Matrix4::MultiplyVector(test3, matrixPosition);

				pModel->SetWorldTransform(worldTransform);
				pModel->Render(pRenderer);
			}

			if (cell.m_wallID[1] != 0)
			{
				// left.
				std::map<u32, RenderModel*>::iterator it = m_mapModels.find(cell.m_wallID[1]);
				assert(it != m_mapModels.end());
				RenderModel* pModel = it->second;

				const std::vector<RenderModel_Mesh>& meshes = pModel->GetMeshes();
				assert(!meshes.empty());
				const RenderModel_Mesh& mesh = meshes.front();

				// center it, still in directX coords.
				Matrix4 matrixCenter;
				matrixCenter.SetTranslation(Vector3(0.f - ((mesh.m_max.x + mesh.m_min.x) / 2.f), 0.f - mesh.m_min.y, 0.f - ((mesh.m_max.z + mesh.m_min.z) / 2.f)));

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
				matrixRotate.SetRotate(Vector3(0.f, 0.f, DirectX::XM_PI * static_cast<f32>(90.f) / 180.f));

				// position it.
				Matrix4 matrixPosition;
				matrixPosition.SetTranslation(Vector3(static_cast<f32>(cellPos.x), static_cast<f32>(cellPos.y) + 0.5f, 0.f));

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

				pModel->SetWorldTransform(worldTransform);
				pModel->Render(pRenderer);
			}

			if (cell.m_wallID[2] != 0)
			{
				// right.
				std::map<u32, RenderModel*>::iterator it = m_mapModels.find(cell.m_wallID[2]);
				assert(it != m_mapModels.end());
				RenderModel* pModel = it->second;

				const std::vector<RenderModel_Mesh>& meshes = pModel->GetMeshes();
				assert(!meshes.empty());
				const RenderModel_Mesh& mesh = meshes.front();

				// center it, still in directX coords.
				Matrix4 matrixCenter;
				matrixCenter.SetTranslation(Vector3(0.f - ((mesh.m_max.x + mesh.m_min.x) / 2.f), 0.f - mesh.m_min.y, 0.f - ((mesh.m_max.z + mesh.m_min.z) / 2.f)));

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
				matrixRotate.SetRotate(Vector3(0.f, 0.f, DirectX::XM_PI* static_cast<f32>(-90.f) / 180.f));

				// position it.
				Matrix4 matrixPosition;
				matrixPosition.SetTranslation(Vector3(static_cast<f32>(cellPos.x + 1), static_cast<f32>(cellPos.y) + 0.5f, 0.f));

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

				pModel->SetWorldTransform(worldTransform);
				pModel->Render(pRenderer);
			}

			if (cell.m_wallID[3] != 0)
			{
				// bottom.
				std::map<u32, RenderModel*>::iterator it = m_mapModels.find(cell.m_wallID[3]);
				assert(it != m_mapModels.end());
				RenderModel* pModel = it->second;

				const std::vector<RenderModel_Mesh>& meshes = pModel->GetMeshes();
				assert(!meshes.empty());
				const RenderModel_Mesh& mesh = meshes.front();

				// center it, still in directX coords.
				Matrix4 matrixCenter;
				matrixCenter.SetTranslation(Vector3(0.f - ((mesh.m_max.x + mesh.m_min.x) / 2.f), 0.f - mesh.m_min.y, 0.f - ((mesh.m_max.z + mesh.m_min.z) / 2.f)));

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
				matrixRotate.SetRotate(Vector3(0.f, 0.f, DirectX::XM_PI* static_cast<f32>(180.f) / 180.f));

				// position it.
				Matrix4 matrixPosition;
				matrixPosition.SetTranslation(Vector3(static_cast<f32>(cellPos.x) + 0.5f, static_cast<f32>(cellPos.y + 1), 0.f));

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

				pModel->SetWorldTransform(worldTransform);
				pModel->Render(pRenderer);
			}
		}
	}

	// draw the character.
	{
		const std::vector<RenderModel_Mesh>& meshes = m_pCharacterModel->GetMeshes();
		assert(!meshes.empty());
		const RenderModel_Mesh& mesh = meshes.front();

		// center it, still in directX coords.
		Matrix4 matrixCenter;
		matrixCenter.SetTranslation(Vector3(0.f - ((mesh.m_max.x + mesh.m_min.x) / 2.f), 0.f - mesh.m_min.y, 0.f - ((mesh.m_max.z + mesh.m_min.z) / 2.f)));

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
		matrixRotate.SetRotate(Vector3(0.f, 0.f, DirectX::XM_PI * (m_characterFacing - 90.f) / 180.f));

		// position it.
		Matrix4 matrixPosition;
		matrixPosition.SetTranslation(m_characterPosition);

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

		m_pCharacterModel->SetWorldTransform(worldTransform);
		m_pCharacterModel->Render(pRenderer);
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

		for (std::vector<MapCell>::iterator it = m_map.begin(); it != m_map.end(); ++it)
		{
			MapCell& cell = *it;
			cell.m_tileID = defaultTile;
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
					MapCell& cell = m_map[(pos.y * m_mapSize.x) + pos.x];
					if (_strcmpi(values[0].c_str(), "top") == 0)
					{
						cell.m_wallID[0] = atol(values[1].c_str());
					}
					else if (_strcmpi(values[0].c_str(), "left") == 0)
					{
						cell.m_wallID[1] = atol(values[1].c_str());
					}
					else if (_strcmpi(values[0].c_str(), "right") == 0)
					{
						cell.m_wallID[2] = atol(values[1].c_str());
					}
					else if (_strcmpi(values[0].c_str(), "bottom") == 0)
					{
						cell.m_wallID[3] = atol(values[1].c_str());
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
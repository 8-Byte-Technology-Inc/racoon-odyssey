#include "pch.h"

#include "render/RenderMain.h"
#include "render/RenderModel.h"
#include "render/RenderTexture.h"
#include "render/RenderStatusBars.h"

#include "Avatar.h"

namespace TB8
{

const f32 MAX_VELOCITY = 5.f;

World_Avatar* World_Avatar::Alloc(Client_Globals* pGlobalState, const char* pszCharacterModelPath)
{
	World_Avatar* pObj = TB8_NEW(World_Avatar)(pGlobalState);
	pObj->__Initialize(pszCharacterModelPath);
	return pObj;
}

void World_Avatar::Free()
{
	TB8_DEL(this);
}

World_Avatar::World_Avatar(Client_Globals* pGlobalState)
	: World_Unit(pGlobalState)
	, m_pStatusBars(nullptr)
	, m_restingFrameCount(0)
	, m_digestingFrameCount(0)
	, m_distanceTravelledLast(0.f)
{
}

World_Avatar::~World_Avatar()
{
	__Uninitialize();
}

void World_Avatar::__Initialize(const char* pszCharacterModelPath)
{
	World_Unit::__Initialize();

	// open status bar texture associated with this model.
	std::string textureFile = pszCharacterModelPath;
	File::StripFileNameFromPath(textureFile);
	File::AppendToPath(textureFile, "status_bar.png");
	std::string texturePath = __GetPathAssets();
	File::AppendToPath(texturePath, textureFile.c_str());
	RenderTexture* pTexture = RenderTexture::Alloc(__GetRenderer(), texturePath.c_str());

	m_pStatusBars = RenderStatusBars::Alloc(__GetRenderer());
	m_barID_Chill = m_pStatusBars->AddBar("Chillaxness", pTexture, Vector2(0.f, 0.f), Vector2(1.f, 0.25f), 5, 20, 2);
	m_barID_Spunk = m_pStatusBars->AddBar("Spunkiness", pTexture, Vector2(0.f, 0.25f), Vector2(1.f, 0.50f), 20, 40, 4);
	m_barID_Full = m_pStatusBars->AddBar("Fullness", pTexture, Vector2(0.f, 0.50f), Vector2(1.f, 0.75f), 26, 30, 3);
	m_barID_Gas = m_pStatusBars->AddBar("Gassiness", pTexture, Vector2(0.f, 0.75f), Vector2(1.f, 1.f), 2, 10, 1);

	RELEASEI(pTexture);

	m_maxVelocity = MAX_VELOCITY;
}

void World_Avatar::__Uninitialize()
{
	RELEASEI(m_pStatusBars);
}

void World_Avatar::Update(s32 frameCount)
{
	World_Unit::Update(frameCount);

	if (IsSitting())
	{
		// if resting a while, increase spunk.
		m_restingFrameCount += frameCount;
		if (m_restingFrameCount > 120)
		{
			m_pStatusBars->SetBarValueDelta(m_barID_Spunk, +1);
			m_restingFrameCount = 0;
			__UpdateVelocity();
		}

		// if resting a while, digest some food.
		m_digestingFrameCount += frameCount;
		if (m_digestingFrameCount > 120)
		{
			m_pStatusBars->SetBarValueDelta(m_barID_Full, -1);
			m_pStatusBars->SetBarValueDelta(m_barID_Gas, +1);
			m_digestingFrameCount = 0;
		}
	}
	else
	{
		// if moving, reduce spunk.
		while ((m_distanceTravelled - m_distanceTravelledLast) > 0.25f)
		{
			m_pStatusBars->SetBarValueDelta(m_barID_Spunk, -1);
			m_distanceTravelledLast += 0.25f;
			__UpdateVelocity();
		}
	}
}

void World_Avatar::__UpdateVelocity()
{
	if (m_pStatusBars->GetBarValue(m_barID_Spunk) >= 8)
	{
		m_maxVelocity = MAX_VELOCITY;
	}
	else if (m_pStatusBars->GetBarValue(m_barID_Spunk) > 4)
	{
		m_maxVelocity = MAX_VELOCITY * 0.2f;
	}
	else if (m_pStatusBars->GetBarValue(m_barID_Spunk) > 0)
	{
		m_maxVelocity = MAX_VELOCITY * 0.1f;
	}
	else
	{
		m_maxVelocity = 0.f;
	}
}

void World_Avatar::Render2D(const Vector3& screenWorldPos)
{
	World_Unit::Render2D(screenWorldPos);

	m_pStatusBars->Render2D();
}

void World_Avatar::Render3D(const Vector3& screenWorldPos)
{
	World_Unit::Render3D(screenWorldPos);

	m_pStatusBars->Render3D();
}

}

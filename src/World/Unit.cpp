#include "pch.h"

#include "render/RenderMain.h"
#include "render/RenderModel.h"
#include "render/RenderImagine.h"

#include "Unit.h"

namespace TB8
{

const s32 SITTING_FRAME_MAX = 30;

const s32 THOUGHT_FRAME_SHOW_MIN = 60 * 5;
const s32 THOUGHT_FRAME_SHOW_MAX = 60 * 10;

const s32 THOUGHT_FRAME_HIDE_MIN = 60 * 10;
const s32 THOUGHT_FRAME_HIDE_MAX = 60 * 20;

static const char* s_mooeyThoughts[] =
{
	"I wonder where little lion and tiger are?",
	"How did I get in this stone pen?",
	"I'm really sleepy, and I have no idea where I am.",
	"That grass over there looks tasty!",
	"I miss my brothers and sisters",
	"The farmer's garden has delicious vegtables!",
	"I miss my bed ...",
	"I wonder if the farmer's blueberries are ripe?",
	"I wonder where I am?  I can't see the barn from here.",
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Morbi non arcu risus quis varius quam quisque id. Elit eget gravida cum sociis natoque penatibus et magnis. Enim eu turpis egestas pretium aenean pharetra. Maecenas volutpat blandit aliquam etiam erat velit scelerisque in dictum. Senectus et netus et malesuada fames ac. Nunc sed augue lacus viverra vitae congue eu. Integer feugiat scelerisque varius morbi enim nunc faucibus. Quis eleifend quam adipiscing vitae proin sagittis. Commodo ullamcorper a lacus vestibulum sed arcu non odio euismod. Varius quam quisque id diam. Odio euismod lacinia at quis risus sed. Feugiat pretium nibh ipsum consequat nisl vel.",
};

World_Unit* World_Unit::Alloc(Client_Globals* pGlobalState)
{
	World_Unit* pObj = TB8_NEW(World_Unit)(pGlobalState);
	pObj->__Initialize();
	return pObj;
}

void World_Unit::Free()
{
	TB8_DEL(this);
}

World_Unit::~World_Unit()
{
	RELEASEI(m_pImagine);
}

void World_Unit::__Initialize()
{
	m_sittingFrame = SITTING_FRAME_MAX;

	m_thoughtFrame = 0;
	m_thoughtFrameNext = (THOUGHT_FRAME_HIDE_MIN + (rand() % (THOUGHT_FRAME_HIDE_MAX - THOUGHT_FRAME_HIDE_MIN)));
	m_thoughtFrequency.resize(ARRAYSIZE(s_mooeyThoughts));
}

void World_Unit::Render2D(const Vector3& screenWorldPos)
{
	if (m_pImagine)
	{
		const Vector3 renderWorldPos = m_pos - screenWorldPos;
		const Vector3 renderWorldPosUL(renderWorldPos.x, renderWorldPos.y, renderWorldPos.z + (m_size.y * m_scale));
		const Vector2 screenPos = __GetRenderer()->WorldToScreenCoords(renderWorldPosUL);

		m_pImagine->SetPosition(screenPos);
		m_pImagine->Render();
	}
}

void World_Unit::ComputeNextPosition(s32 frameCount, Vector3& pos, Vector3& vel)
{
	// compute environmental forces.
	const f32 forceFactor = 20.f;
	const f32 forceDampen = 10.0f;
	const f32 windResistFactor = 0.25f;
	const f32 gravityFactor = 10.0f;
	const f32 elapsedTime = static_cast<f32>(frameCount) / 60.f;

	// process jumping.
	if (!is_approx_zero(m_force.z))
	{
		if (is_approx_zero(m_pos.z))
		{
			m_velocity.z = +4.f;
		}
		m_force.z = 0.f;
	}

	const Vector3 forceChar = m_force * forceFactor;

	const f32 velTotalSq = m_velocity.MagSq();

	Vector3 forceEnv;
	forceEnv.x = (forceDampen + (velTotalSq * windResistFactor)) * get_sign(m_velocity.x) * -1.f;
	forceEnv.y = (forceDampen + (velTotalSq * windResistFactor)) * get_sign(m_velocity.y) * -1.f;
	forceEnv.z = (m_mass * gravityFactor) * -1.f;

	// compute net force.
	const Vector3 forceNet = forceChar + forceEnv;

	// compute accel.
	const Vector3 accel = forceNet / m_mass;

	// compute new velocity.
	vel = m_velocity + (accel * elapsedTime);

	// don't exceed max velocity, unless it's gravity.
	if (vel.MagSq() > (m_maxVelocity * m_maxVelocity))
	{
		const Vector3 velN = vel.Normalize(vel) * m_maxVelocity;
		if (vel.z < 0.f)
		{
			vel.x = velN.x;
			vel.y = velN.y;
		}
		else
		{
			vel = velN;
		}
	}

	if (is_approx_zero(forceChar.x))
	{
		if (is_approx_zero(m_velocity.x))
			vel.x = 0.f;
		else if ((vel.x < 0.f) && (m_velocity.x > 0.f))
			vel.x = 0.f;
		else if ((vel.x > 0.f) && (m_velocity.x < 0.f))
			vel.x = 0.f;
	}

	if (is_approx_zero(forceChar.y))
	{
		if (is_approx_zero(m_velocity.y))
			vel.y = 0.f;
		else if ((vel.y < 0.f) && (m_velocity.y > 0.f))
			vel.y = 0.f;
		else if ((vel.y > 0.f) && (m_velocity.y < 0.f))
			vel.y = 0.f;
	}

	const Vector3& vel0 = m_velocity;
	const Vector3& vel1 = vel;

	// compute new position.
	pos = m_pos + (vel0 + ((vel1 - vel0) * 0.5f)) * elapsedTime;

	// can't go lower than 0.
	if (pos.z < 0.f)
	{
		pos.z = 0.f;
		vel.z = 0.f;
	}
}

void World_Unit::UpdatePosition(s32 frameCount, const Vector3& pos, const Vector3& vel)
{
	// compute new facing.
	f32 facing = m_rotation;
	if (!is_approx_zero(vel.x) || !is_approx_zero(vel.y))
	{
		Vector2 facingVector(vel.x, vel.y);
		facingVector.Normalize();
		const f32 angle = std::atan2(facingVector.y, facingVector.x);
		facing = (angle / DirectX::XM_PI) * 180.f;
	}

	// compute distance traveled.
	const Vector3 vDist = pos - m_pos;
	const f32 dist = vDist.Mag();

	// compute new animation.
	if (!is_approx_zero(dist))
	{
		if (m_sittingFrame > 0)
		{
			RELEASEI(m_pImagine);

			m_sittingFrame -= (frameCount * 2);

			if (m_sittingFrame > 0)
			{
				u32 animCount = m_pModel->GetAnimCount();
				const f32 animRange = static_cast<f32>(animCount);
				const u32 animIndex0 = static_cast<u32>(m_animIndex * animRange) % animCount;
				const f32 t = static_cast<f32>(SITTING_FRAME_MAX - m_sittingFrame) / static_cast<f32>(SITTING_FRAME_MAX);

				__InterpolateAnims(-1, 1, t);
				__InterpolateCenterAndSize(-1, 1, t);
				__ComputeModelWorldLocalTransform();
				return;
			}

			m_sittingFrame = 0;
			m_animIndex = 0.f;

			__ComputeModelAnimCenterAndSize(1);
			__ComputeModelWorldLocalTransform();
		}

		// udpate anim index.
		m_animIndex += dist;
		while (m_animIndex > 1.f)
			m_animIndex -= 1.f;

		// decide which two animations we'll interpolate between.
		u32 animCount = m_pModel->GetAnimCount();
		const f32 animRange = static_cast<f32>(animCount);
		const u32 animIndex0 = static_cast<u32>(m_animIndex * animRange) % animCount;
		const u32 animIndex1 = (animIndex0 + 1) % animCount;
		const f32 t = (m_animIndex - (static_cast<f32>(animIndex0) / animRange)) * animRange;

		__InterpolateAnims(animIndex0 + 1, animIndex1 + 1, t);

		// update position, rotation, velocity.
		m_pos = pos;
		m_rotation = facing;
		m_velocity = vel;

		// update distance travelled.
		m_distanceTravelled += dist;
	}
	else if (m_sittingFrame < SITTING_FRAME_MAX)
	{
		// no longer moving, sit down.
		m_sittingFrame += frameCount;

		if (m_sittingFrame < SITTING_FRAME_MAX)
		{
			u32 animCount = m_pModel->GetAnimCount();
			const f32 animRange = static_cast<f32>(animCount);
			const u32 animIndex0 = static_cast<u32>(m_animIndex * animRange) % animCount;
			const f32 t = static_cast<f32>(m_sittingFrame) / static_cast<f32>(SITTING_FRAME_MAX);

			__InterpolateAnims(animIndex0 + 1, -1, t);
			__InterpolateCenterAndSize(animIndex0 + 1, -1, t);
			__ComputeModelWorldLocalTransform();
			return;
		}

		m_sittingFrame = SITTING_FRAME_MAX;
		m_animIndex = 0.f;

		__SetAnim(-1);
		__ComputeModelAnimCenterAndSize(-1);
		__ComputeModelWorldLocalTransform();

		m_thoughtFrame = 0;
		m_thoughtFrameNext = (THOUGHT_FRAME_HIDE_MIN + (rand() % (THOUGHT_FRAME_HIDE_MAX - THOUGHT_FRAME_HIDE_MIN)));
	}

	// compute updated bounds.
	ComputeBounds();
}

void World_Unit::Update(s32 frameCount)
{
	if (IsSitting())
	{
		m_thoughtFrame += frameCount;
		const bool change = m_thoughtFrame > m_thoughtFrameNext;
		if (change)
		{
			if (m_pImagine)
			{
				RELEASEI(m_pImagine);

				m_thoughtFrame = 0;
				m_thoughtFrameNext = (THOUGHT_FRAME_HIDE_MIN + (rand() % (THOUGHT_FRAME_HIDE_MAX - THOUGHT_FRAME_HIDE_MIN)));
			}
			else
			{
				u32 thoughtFreqMax = UINT_MAX;
				for (u32 i = 0; i < m_thoughtFrequency.size(); ++i)
				{
					thoughtFreqMax = std::min<u32>(thoughtFreqMax, m_thoughtFrequency[i]);
				}

				std::vector<u32> thoughtIndicies;
				for (u32 i = 0; i < m_thoughtFrequency.size(); ++i)
				{
					if (m_thoughtFrequency[i] <= (thoughtFreqMax + 1))
					{
						thoughtIndicies.push_back(i);
					}
				}

				const u32 iiThought = rand() % thoughtIndicies.size();
				const u32 iThought = thoughtIndicies[iiThought];
				const RenderImagine_Type t = (rand() & 1) ? RenderImagine_Type_Imagine : RenderImagine_Type_Speech;
				m_pImagine = RenderImagine::Alloc(__GetRenderer());
				m_pImagine->SetType(t);
				m_pImagine->SetText(s_mooeyThoughts[iThought]);
				m_thoughtFrequency[iThought]++;

				m_thoughtFrame = 0;
				m_thoughtFrameNext = (THOUGHT_FRAME_SHOW_MIN + (rand() % (THOUGHT_FRAME_SHOW_MAX - THOUGHT_FRAME_SHOW_MIN)));
			}
		}
	}
}

bool World_Unit::IsSitting() const
{
	return m_sittingFrame == SITTING_FRAME_MAX;
}

}

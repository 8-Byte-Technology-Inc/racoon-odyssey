#include "pch.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "common/file_io.h"
#include "common/parse_xml.h"

#include "RenderHelper.h"
#include "RenderModel.h"
#include "RenderTexture.h"
#include "RenderShaders.h"
#include "RenderMain.h"

namespace TB8
{

typedef bool(*fnParseChars)(RenderModel_DAE_ParseContext* ctx, char v);
typedef void(*fnParseBuffer)(RenderModel_DAE_ParseContext* ctx);
typedef void(*fnParseDataSet)(RenderModel_DAE_ParseContext* ctx);

enum RenderModel_DAE_InputSemantic : s32
{
	RenderModel_DAE_InputSemantic_Vertex,
	RenderModel_DAE_InputSemantic_Normal,
	RenderModel_DAE_InputSemantic_TexCoord,
};

struct RenderModel_DAE_Texture
{
	std::string m_id;
	u32 m_index;
	std::string m_path;
};

struct RenderModel_DAE_Effect
{
	std::string m_id;
	DirectX::XMFLOAT4 m_emission;
	DirectX::XMFLOAT4 m_diffuse;
	DirectX::XMFLOAT4 m_specular;
	std::string m_textureID;
	s32 m_textureIndex;

	RenderModel_DAE_Effect()
	{
		Clear();
	}

	void Clear()
	{
		m_id.clear();
		ZeroMemory(&m_emission, sizeof(m_emission));
		ZeroMemory(&m_diffuse, sizeof(m_diffuse));
		ZeroMemory(&m_specular, sizeof(m_specular));
		m_textureID.clear();
		m_textureIndex = -1;
	}
};

struct RenderModel_DAE_Material
{
	RenderModel_DAE_Material()
		: m_effectIndex(-1)
	{
	}
	std::string m_id;
	s32 m_effectIndex;
};

struct RenderModel_DAE_SkinJoint_Anim_Transform
{
	RenderModel_DAE_SkinJoint_Anim_Transform()
		: m_animID(-1)
		, m_isModelMatrixComputed(false)
	{
	}
	s32						m_animID;
	bool					m_isModelMatrixComputed;
	Matrix4					m_modelMatrix;
};

struct RenderModel_DAE_SkinJoint
{
	RenderModel_DAE_SkinJoint()
		: m_pMesh(nullptr)
		, m_index(-1)
		, m_parent(-1)
		, m_isModelMatrixComputed(false)
	{
	}

	RenderModel_DAE_SkinJoint_Anim_Transform* GetTransformByAnimID(s32 animID)
	{
		for (std::vector<RenderModel_DAE_SkinJoint_Anim_Transform>::iterator it = m_animModelMatrix.begin(); it != m_animModelMatrix.end(); ++it)
		{
			if ((*it).m_animID == animID)
				return &(*it);
		}
		return nullptr;
	}

	RenderModel_DAE_Mesh*	m_pMesh;
	std::string				m_name;
	s32						m_index;
	s32						m_parent;
	std::vector<s32>		m_children;
	Matrix4					m_jointMatrix;
	Matrix4					m_invBindMatrix;

	bool					m_isModelMatrixComputed;
	Matrix4					m_modelMatrix;
	std::vector<RenderModel_DAE_SkinJoint_Anim_Transform>	m_animModelMatrix;
};

struct RenderModel_DAE_SkinWeight
{
	RenderModel_DAE_SkinWeight()
		: m_weight(0.f)
	{
	}
	f32				m_weight;
};

struct RenderModel_DAE_Anim_Transform
{
	s32						m_animID;
	Matrix4					m_jointMatrix;
};

struct RenderModel_DAE_Anim
{
	std::string										m_jointName;
	std::vector<RenderModel_DAE_Anim_Transform>		m_transforms;
};

struct RenderModel_DAE_Vertex_SkinJointWeight
{
	RenderModel_DAE_Vertex_SkinJointWeight()
		: m_jointIndex(-1)
		, m_weight(0.f)
	{
	}
	s32				m_jointIndex;
	f32				m_weight;

	static bool Sort(const RenderModel_DAE_Vertex_SkinJointWeight& lhs, const RenderModel_DAE_Vertex_SkinJointWeight& rhs)
	{
		return lhs.m_weight > rhs.m_weight;
	}
};

struct RenderModel_DAE_Vertex
{
	Vector3													m_pos;
	std::vector<RenderModel_DAE_Vertex_SkinJointWeight>		m_weights;
};

struct RenderModel_DAE_Triangle_Vertex
{
	RenderModel_DAE_Triangle_Vertex()
		: m_vertexIndex(-1)
		, m_normalIndex(-1)
		, m_mapIndex(-1)
	{
	}
	s32 m_vertexIndex;
	s32 m_normalIndex;
	s32 m_mapIndex;
};

struct RenderModel_DAE_Triangle
{
	RenderModel_DAE_Triangle()
		: m_materialIndex(-1)
	{
	}
	RenderModel_DAE_Triangle_Vertex m_vertex[3];
	s32 m_materialIndex;
};

struct RenderModel_DAE_Mesh
{
	RenderModel_DAE_Mesh()
	{
	}

	std::string								m_name;
	std::string								m_id;
	std::string								m_skinName;

	RenderModel_Bounds						m_bounds;

	std::vector<RenderModel_DAE_Vertex>		m_meshVerticies;
	std::vector<Vector3>					m_meshNormals;
	std::vector<RenderModel_DAE_Triangle>	m_meshTriangles;
	std::vector<Vector2>					m_meshMap;

	Matrix4									m_bindShapeMatrix;
	std::vector<RenderModel_DAE_SkinJoint>	m_skinJoints;
	std::vector<RenderModel_DAE_SkinWeight> m_skinWeights;
	std::vector<s32>						m_vertexWeightCounts;
};

struct RenderModel_DAE_ParseContext
{
	RenderModel_DAE_ParseContext(RenderMain* pRenderer, RenderModel* pModel, const char* _modelName)
		: m_pRenderer(pRenderer)
		, m_pModel(pModel)
		, m_pMesh(nullptr)
		, m_materialIndex(-1)
		, m_vertexIndex(-1)
		, m_jointIndex(-1)
		, m_animIndex(-1)
		, m_animIDGen(0)
		, m_modelUp(0.f, 0.f, 1.f)
		, m_fnParseChars(nullptr)
		, m_fnParseBuffer(nullptr)
		, m_fnParseDataSet(nullptr)
	{
		m_meshes.reserve(16);
		const char* itS = _modelName;
		const char* itE = itS;
		while (*itE)
		{
			if (*itE == ',' || *itE == ';')
			{
				if (itS != itE)
				{
					char name[256];
					memcpy(name, itS, itE - itS);
					name[itE - itS] = 0;

					m_meshes.emplace_back();
					m_meshes.back().m_name = name;
				}
				itS = itE + 1;
				itE = itS;
			}
			else
			{
				++itE;
			}
		}
		if (itS != itE)
		{
			m_meshes.emplace_back();
			m_meshes.back().m_name = itS;
		}
	}

	std::string m_assetPath;

	RenderMain* m_pRenderer;
	RenderModel* m_pModel;

	std::vector<std::string> m_tagStack;
	std::vector<std::pair<std::string, std::string>> m_idStack;
	std::vector<std::pair<std::string, Matrix4>> m_transformStack;
	std::vector<RenderModel_DAE_SkinJoint*> m_skinJointStack;

	RenderModel_DAE_Mesh*				m_pMesh;
	s32 m_materialIndex;
	s32 m_vertexIndex;
	std::vector<RenderModel_DAE_InputSemantic> m_inputSemantic;
	s32 m_jointIndex;
	s32 m_animIndex;
	Matrix4 m_coordTranslate;
	Vector3 m_modelUp;

	std::string m_buffer;
	std::vector<f32> m_f32;
	std::vector<s32> m_s32;
	std::vector<s32> m_tempAnimIDs;
	std::vector<std::string> m_strings;
	RenderModel_DAE_Effect m_tempEffect;

	fnParseChars m_fnParseChars;
	fnParseBuffer m_fnParseBuffer;
	fnParseDataSet m_fnParseDataSet;

	Matrix4									m_baseJointMatrix;
	std::vector<RenderModel_DAE_Texture>	m_textures;
	std::vector<RenderModel_DAE_Effect>		m_effects;
	std::vector<RenderModel_DAE_Material>	m_materials;
	std::vector<RenderModel_DAE_Mesh>		m_meshes;

	s32										m_animIDGen;
	std::vector<RenderModel_DAE_Anim>		m_anims;
	std::map<f32, s32>						m_animIDMap;
};

void RenderModel::__InitializeFromDAE(RenderMain* pRenderer, const char* path, const char* filename, const char* modelName)
{
	HRESULT hr = S_OK;

	std::string modelFilePath = path;
	TB8::File::AppendToPath(modelFilePath, filename);

	// open the xml file.
	TB8::File* f = TB8::File::AllocOpen(modelFilePath.c_str(), true);
	assert(f);

	RenderModel_DAE_ParseContext userCtx(pRenderer, this, modelName);

	userCtx.m_assetPath = modelFilePath;
	TB8::File::StripFileNameFromPath(userCtx.m_assetPath);

	{
		XML_Parser parser;
		parser.SetHandlers(std::bind(__ParseDAEStartElement, &userCtx, std::placeholders::_1, std::placeholders::_2),
							std::bind(__ParseDAECharacters, &userCtx, std::placeholders::_1, std::placeholders::_2),
							std::bind(__ParseDAEEndElement, &userCtx, std::placeholders::_1));
		parser.SetReader(std::bind(__ParseDAERead, f, std::placeholders::_1, std::placeholders::_2));
		parser.Parse();
	}

	OBJFREE(f);

	// quick ref to primary mesh.
	RenderModel_DAE_Mesh& primaryMesh = userCtx.m_meshes.front();

	// compute center.
	const RenderModel_NamedVertex* pCenterVertex = RenderModel::__FindNamedVertex("Base_Center");
	if (pCenterVertex)
	{
		m_center = Vector3(pCenterVertex->m_vertex.x, pCenterVertex->m_vertex.y, (primaryMesh.m_bounds.m_min.z + primaryMesh.m_bounds.m_max.z) / 2.f);
	}
	else
	{
		primaryMesh.m_bounds.ComputeCenterAndSize();
		m_center = primaryMesh.m_bounds.m_center;
	}

	// set up coordinate translation.
	{
		Matrix4 modelCenter;
		modelCenter.SetTranslation(Vector3(-m_center.x, -m_center.y, -m_center.z));

		m_coordTranslate = Matrix4::MultiplyAB(userCtx.m_coordTranslate, modelCenter);
	}

	// connect effect textures.
	for (std::vector<RenderModel_DAE_Effect>::iterator itEffect = userCtx.m_effects.begin(); itEffect != userCtx.m_effects.end(); ++itEffect)
	{
		RenderModel_DAE_Effect& effect = *itEffect;
		if (!effect.m_textureID.empty())
		{
			for (std::vector<RenderModel_DAE_Texture>::iterator itTexture = userCtx.m_textures.begin(); itTexture != userCtx.m_textures.end(); ++itTexture)
			{
				RenderModel_DAE_Texture& texture = *itTexture;
				if (texture.m_id != effect.m_textureID)
					continue;
				effect.m_textureIndex = static_cast<s32>(texture.m_index);
				break;
			}
		}
	}

	// build list of meshes.
	for (std::vector<RenderModel_DAE_Mesh>::iterator itMesh = userCtx.m_meshes.begin(); itMesh != userCtx.m_meshes.end(); ++itMesh)
	{
		RenderModel_DAE_Mesh& srcMesh = *itMesh;
		m_meshes.emplace_back();
		RenderModel_Mesh& dstMesh = m_meshes.back();

		const Vector3 vmin = Matrix4::MultiplyVector(srcMesh.m_bounds.m_min, m_coordTranslate);
		const Vector3 vmax = Matrix4::MultiplyVector(srcMesh.m_bounds.m_max, m_coordTranslate);

		dstMesh.m_name = srcMesh.m_name;

		dstMesh.m_bounds.m_min.x = std::min<f32>(vmin.x, vmax.x);
		dstMesh.m_bounds.m_min.y = std::min<f32>(vmin.y, vmax.y);
		dstMesh.m_bounds.m_min.z = std::min<f32>(vmin.z, vmax.z);
		dstMesh.m_bounds.m_max.x = std::max<f32>(vmin.x, vmax.x);
		dstMesh.m_bounds.m_max.y = std::max<f32>(vmin.y, vmax.y);
		dstMesh.m_bounds.m_max.z = std::max<f32>(vmin.z, vmax.z);

		dstMesh.m_bounds.ComputeCenterAndSize();
	}

	// get a reference to the shader.
	m_pShader = pRenderer->GetShaderByID(RenderShaderID_Generic);
	m_pShader->AddRef();

	// cache joints.
	if (!userCtx.m_meshes.empty())
	{
		RenderModel_DAE_Mesh& mesh = userCtx.m_meshes.front();
		m_cJoints = static_cast<u32>(mesh.m_skinJoints.size());
		assert(m_cJoints <= ARRAYSIZE(m_joints));
		if (m_cJoints > 0)
		{
			m_baseJointMatrix = userCtx.m_baseJointMatrix;
			m_bindShapeMatrix = mesh.m_bindShapeMatrix;

			Matrix4 x1 = Matrix4::MultiplyAB(mesh.m_skinJoints[0].m_jointMatrix, mesh.m_skinJoints[0].m_invBindMatrix);
			Matrix4 x2 = Matrix4::MultiplyAB(m_baseJointMatrix, mesh.m_bindShapeMatrix);

			for (std::vector<RenderModel_DAE_SkinJoint>::iterator itJoint = mesh.m_skinJoints.begin(); itJoint != mesh.m_skinJoints.end(); ++itJoint)
			{
				const RenderModel_DAE_SkinJoint& src = *itJoint;
				assert(src.m_index < ARRAYSIZE(m_joints));
				assert(src.m_parent < src.m_index);
				RenderModel_Joint& dst = m_joints[src.m_index];

				dst.m_name = src.m_name;

				dst.m_index = src.m_index;
				dst.m_parentIndex = src.m_parent;

				dst.m_baseMatrix = src.m_jointMatrix;
				dst.m_baseInvBindMatrix = src.m_invBindMatrix;

				dst.m_isDirty = true;
				dst.m_effectiveMatrix = dst.m_baseMatrix;
				dst.m_computedMatrix.SetIdentity();
			}
		}
	}

	// setup anims.
	if (!userCtx.m_animIDMap.empty())
	{
		{
			m_anims.emplace_back();
			RenderModel_Anim& anim = m_anims.back();
			anim.m_animIndex = static_cast<s32>(m_anims.size()) - 1;
			anim.m_animID = -1;
			anim.m_animTime = -1.f;

			RenderModel_Mesh& mesh = m_meshes.front();
			anim.m_bounds = mesh.m_bounds;
		}

		for (std::map<f32, s32>::iterator it = userCtx.m_animIDMap.begin(); it != userCtx.m_animIDMap.end(); ++it)
		{
			m_anims.emplace_back();
			RenderModel_Anim& anim = m_anims.back();
			anim.m_animIndex = static_cast<s32>(m_anims.size()) - 1;
			anim.m_animID = it->second;
			anim.m_animTime = it->first;

			RenderModel_DAE_Mesh& mesh = userCtx.m_meshes.front();
			for (std::vector<RenderModel_DAE_SkinJoint>::iterator itJoint = mesh.m_skinJoints.begin(); itJoint != mesh.m_skinJoints.end(); ++itJoint)
			{
				const RenderModel_DAE_SkinJoint& srcJoint = *itJoint;
				RenderModel_DAE_Anim_Transform* pTransform = __LookupAnimTransform(userCtx, srcJoint.m_name.c_str(), anim.m_animID);
				if (!pTransform)
					continue;
				anim.m_joints.emplace_back();
				RenderModel_Anim_Joint& dstAnimJoint = anim.m_joints.back();
				dstAnimJoint.m_pJoint = &(m_joints[srcJoint.m_index]);
				dstAnimJoint.m_transform = pTransform->m_jointMatrix;
			}
		}
	}

	// compute animation limits.
	for (std::vector<RenderModel_Anim>::iterator itAnim = m_anims.begin(); itAnim != m_anims.end(); ++itAnim)
	{
		RenderModel_Anim& anim = *itAnim;

		// reset to base transforms.
		ResetJointTransformMatricies();

		// update joint transforms for this animation.
		for (std::vector<RenderModel_Anim_Joint>::iterator itJoint = anim.m_joints.begin(); itJoint != anim.m_joints.end(); ++itJoint)
		{
			RenderModel_Anim_Joint& animJoint = *itJoint;
			RenderModel_Joint& joint = *(animJoint.m_pJoint);
			joint.m_effectiveMatrix = animJoint.m_transform;
			joint.m_isDirty = true;
		}

		// apply transforms.
		__UpdateJointMatricies();

		// go thru verticies, compute limits.
		for (std::vector<RenderModel_DAE_Mesh>::iterator itMesh = userCtx.m_meshes.begin(); itMesh != userCtx.m_meshes.end(); ++itMesh)
		{
			RenderModel_DAE_Mesh& mesh = *itMesh;

			for (std::vector<RenderModel_DAE_Vertex>::const_iterator itVert = mesh.m_meshVerticies.begin(); itVert != mesh.m_meshVerticies.end(); ++itVert)
			{
				const RenderModel_DAE_Vertex& srcVertex = *itVert;
				const Vector3& srcV = srcVertex.m_pos;
				Vector3 dstV;
				for (std::vector<RenderModel_DAE_Vertex_SkinJointWeight>::const_iterator itWeight = srcVertex.m_weights.begin(); itWeight != srcVertex.m_weights.end(); ++itWeight)
				{
					const RenderModel_DAE_Vertex_SkinJointWeight& srcWeight = *itWeight;
					const RenderModel_Joint& joint = m_joints[srcWeight.m_jointIndex];
					const Vector3 dstVComp = Matrix4::MultiplyVector(srcV, joint.m_boundMatrix) * srcWeight.m_weight;
					dstV += dstVComp;
				}

				const Vector3 dstVDX = Matrix4::MultiplyVector(dstV, m_coordTranslate);
				anim.m_bounds.AddVector(dstVDX);
			}
		}

		anim.m_bounds.ComputeCenterAndSize();
	}

	// reset to base transforms.
	ResetJointTransformMatricies();

	// setup bone texture.
	if (!m_anims.empty())
	{
		const s32 animCount = static_cast<s32>(m_anims.size());
		s32 boneCount = 0;
		for (std::vector<RenderModel_DAE_Mesh>::iterator itMesh = userCtx.m_meshes.begin(); itMesh != userCtx.m_meshes.end(); ++itMesh)
		{
			RenderModel_DAE_Mesh& mesh = *itMesh;
			boneCount += static_cast<s32>(mesh.m_skinJoints.size());
		}

		if (boneCount > 0)
		{
			const IVector2 boneTextureSize(1 + animCount * 8, 1 + boneCount * 4);
			const s32 boneTextureDataSize = boneTextureSize.y * boneTextureSize.x * sizeof(f32);
			f32* pBoneTextureData = reinterpret_cast<f32*>(TB8_MALLOC(boneTextureDataSize));

			// put sizes in first column.
			*(pBoneTextureData + 0 * boneTextureSize.x) = static_cast<f32>(animCount);
			*(pBoneTextureData + 1 * boneTextureSize.x) = static_cast<f32>(boneCount);
			for (s32 i = 2; i < boneTextureSize.y; ++i)
			{
				*(pBoneTextureData + i * boneTextureSize.x) = 0.f;
			}

			// add bone animation data for each animation.
			for (std::vector<RenderModel_Anim>::iterator it = m_anims.begin(); it != m_anims.end(); ++it)
			{
				__SetBoneTextureData(userCtx, pBoneTextureData, boneTextureSize, *it);
			}

			D3D11_TEXTURE2D_DESC desc;
			desc.Width = boneTextureSize.x;
			desc.Height = boneTextureSize.y;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R32_TYPELESS;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = pBoneTextureData;
			initialData.SysMemPitch = static_cast<UINT>(boneTextureSize.x * sizeof(f32));
			initialData.SysMemSlicePitch = static_cast<UINT>(boneTextureDataSize);

			ID3D11Texture2D* pTexture = nullptr;
			hr = pRenderer->GetDevice()->CreateTexture2D(&desc, &initialData, &pTexture);
			assert(hr == S_OK);

			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			memset(&SRVDesc, 0, sizeof(SRVDesc));
			SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			SRVDesc.Texture2D.MipLevels = 1;

			assert(m_pBoneTexture == nullptr);
			hr = pRenderer->GetDevice()->CreateShaderResourceView(pTexture, &SRVDesc, &m_pBoneTexture);
			assert(hr == S_OK);

			RELEASEI(pTexture);
			PTRFREE(pBoneTextureData);
		}
	}

	// create model texture.
	if (!userCtx.m_textures.empty())
	{
		std::vector<std::string> texturePaths;
		texturePaths.reserve(userCtx.m_textures.size());
		for (std::vector<RenderModel_DAE_Texture>::iterator it = userCtx.m_textures.begin(); it != userCtx.m_textures.end(); ++it)
		{
			texturePaths.push_back(it->m_path);
		}

		std::vector<RenderTexture_MemoryTexture> memoryTextures;
		m_pTexture = RenderTexture::Alloc(pRenderer, memoryTextures, texturePaths);
	}

	std::vector<RenderShader_Vertex_Generic> verticies;
	std::vector<u16> indicies;
	verticies.reserve(0x10000);
	indicies.reserve(0x10000);

	for (std::vector<RenderModel_DAE_Mesh>::iterator itMesh = userCtx.m_meshes.begin(); itMesh != userCtx.m_meshes.end(); ++itMesh)
	{
		RenderModel_DAE_Mesh& mesh = *itMesh;

		std::vector<RenderModel_DAE_Triangle>::iterator itStart = mesh.m_meshTriangles.begin();
		while (itStart != mesh.m_meshTriangles.end())
		{
			DirectX::XMFLOAT4 color;
			s32 textureIndex = -1;

			// determine color and/or texture for this set of triangles.
			if (itStart->m_materialIndex >= 0)
			{
				RenderModel_DAE_Material& material = userCtx.m_materials[itStart->m_materialIndex];
				if (material.m_effectIndex >= 0)
				{
					RenderModel_DAE_Effect& effect = userCtx.m_effects[material.m_effectIndex];

					const f32 gamma = 1.f / 2.2f;
					color.x = static_cast<f32>(pow(effect.m_diffuse.x + effect.m_specular.x, gamma));
					color.y = static_cast<f32>(pow(effect.m_diffuse.y + effect.m_specular.y, gamma));
					color.z = static_cast<f32>(pow(effect.m_diffuse.z + effect.m_specular.z, gamma));
					color.w = std::min<f32>(/*effect.m_emission.w + */effect.m_diffuse.w + effect.m_specular.w, 1.f);

					textureIndex = effect.m_textureIndex;
				}
			}

			// build the lists.
			std::map<u32, s32> vertexIndexMap;

			std::vector<RenderModel_DAE_Triangle>::iterator itEnd = itStart;
			for (; itEnd != mesh.m_meshTriangles.end() && (itStart->m_materialIndex == itEnd->m_materialIndex); ++itEnd)
			{
				RenderModel_DAE_Triangle triangle = *itEnd;

				for (s32 iVertex = 0; iVertex < 3; ++iVertex)
				{
					RenderModel_DAE_Triangle_Vertex& triangleVertex = triangle.m_vertex[iVertex];

					const RenderModel_DAE_Vertex& srcVertexOriginal = mesh.m_meshVerticies[triangleVertex.m_vertexIndex];

					const Vector3& srcVertex = srcVertexOriginal.m_pos;
					const Vector3& srcNormal = mesh.m_meshNormals[triangleVertex.m_normalIndex];

					DirectX::XMINT4 bones;
					DirectX::XMFLOAT4 weights;
					bones.x = -1;
					bones.y = -1;
					bones.z = -1;
					bones.w = -1;
					weights.x = 0.f;
					weights.y = 0.f;
					weights.z = 0.f;
					weights.w = 0.f;
					assert(srcVertexOriginal.m_weights.size() <= 4);

					if (srcVertexOriginal.m_weights.size() > 0)
					{
						const RenderModel_DAE_Vertex_SkinJointWeight& weight = srcVertexOriginal.m_weights[0];
						bones.x = weight.m_jointIndex;
						weights.x = weight.m_weight;
					}
					if (srcVertexOriginal.m_weights.size() > 1)
					{
						const RenderModel_DAE_Vertex_SkinJointWeight& weight = srcVertexOriginal.m_weights[1];
						bones.y = weight.m_jointIndex;
						weights.y = weight.m_weight;
					}
					if (srcVertexOriginal.m_weights.size() > 2)
					{
						const RenderModel_DAE_Vertex_SkinJointWeight& weight = srcVertexOriginal.m_weights[2];
						bones.z = weight.m_jointIndex;
						weights.z = weight.m_weight;
					}
					if (srcVertexOriginal.m_weights.size() > 3)
					{
						const RenderModel_DAE_Vertex_SkinJointWeight& weight = srcVertexOriginal.m_weights[3];
						bones.w = weight.m_jointIndex;
						weights.w = weight.m_weight;
					}


					s32 index = -1;
					if (textureIndex >= 0)
					{
						// texture.
						Vector2& srcDAETexPos = mesh.m_meshMap[triangleVertex.m_mapIndex];
						Vector2 srcDiretXTexPos;
						srcDiretXTexPos.x = srcDAETexPos.x;
						srcDiretXTexPos.y = 1.f - srcDAETexPos.y;
						Vector2 targetTexPos;
						m_pTexture->MapUV(textureIndex, srcDiretXTexPos, &targetTexPos);

						index = static_cast<s32>(verticies.size());

						// add vertex.
						RenderShader_Vertex_Generic vertex;
						vertex.position.x = srcVertex.x;
						vertex.position.y = srcVertex.y;
						vertex.position.z = srcVertex.z;
						vertex.normal.x = srcNormal.x;
						vertex.normal.y = srcNormal.y;
						vertex.normal.z = srcNormal.z;
						vertex.color.x = targetTexPos.x;
						vertex.color.y = targetTexPos.y;
						vertex.color.z = 0.f;
						vertex.color.w = 0.f;
						vertex.bones = bones;
						vertex.weights = weights;
						verticies.push_back(vertex);
					}
					else
					{
						// color.

						// if we've already added this vertex, we may be able to avoid doing it again if there is no texture.
						const u32 vertexID = static_cast<u32>(triangleVertex.m_vertexIndex) | (static_cast<u32>(triangleVertex.m_normalIndex) << 16);
						std::map<u32, s32>::iterator itVertexIndex = vertexIndexMap.find(vertexID);
						if (itVertexIndex != vertexIndexMap.end())
						{
							index = itVertexIndex->second;
						}
						else
						{
							index = static_cast<s32>(verticies.size());

							// add vertex.
							RenderShader_Vertex_Generic vertex;
							vertex.position.x = srcVertex.x;
							vertex.position.y = srcVertex.y;
							vertex.position.z = srcVertex.z;
							vertex.normal.x = srcNormal.x;
							vertex.normal.y = srcNormal.y;
							vertex.normal.z = srcNormal.z;
							vertex.color = color;
							vertex.bones = bones;
							vertex.weights = weights;
							verticies.push_back(vertex);

							// put it in the map.
							vertexIndexMap.insert(std::make_pair(vertexID, index));
						}
					}

					// add index.
					assert((index >= 0) && (index < 0x10000));
					indicies.push_back(static_cast<u16>(index));
				}
			}

			// next batch.
			vertexIndexMap.clear();
			itStart = itEnd;
		}
	}

	// basic named verticies.
	if (!__FindNamedVertex("Base_Center"))
	{
		RenderModel_NamedVertex nv1;
		nv1.m_name = "Base_Center";
		nv1.m_vertex = Vector3(m_center.x, m_center.y, primaryMesh.m_bounds.m_min.z);
		m_namedVerticies.push_back(nv1);
	}
	{
		RenderModel_NamedVertex nv1;
		nv1.m_name = "Base_UL";
		nv1.m_vertex = Vector3(primaryMesh.m_bounds.m_max.x, primaryMesh.m_bounds.m_min.y, primaryMesh.m_bounds.m_min.z);
		m_namedVerticies.push_back(nv1);
	}
	{
		RenderModel_NamedVertex nv1;
		nv1.m_name = "Base_UR";
		nv1.m_vertex = Vector3(primaryMesh.m_bounds.m_min.x, primaryMesh.m_bounds.m_min.y, primaryMesh.m_bounds.m_min.z);
		m_namedVerticies.push_back(nv1);
	}
	{
		RenderModel_NamedVertex nv1;
		nv1.m_name = "Base_LL";
		nv1.m_vertex = Vector3(primaryMesh.m_bounds.m_max.x, primaryMesh.m_bounds.m_max.y, primaryMesh.m_bounds.m_min.z);
		m_namedVerticies.push_back(nv1);
	}
	{
		RenderModel_NamedVertex nv1;
		nv1.m_name = "Base_LR";
		nv1.m_vertex = Vector3(primaryMesh.m_bounds.m_min.x, primaryMesh.m_bounds.m_max.y, primaryMesh.m_bounds.m_min.z);
		m_namedVerticies.push_back(nv1);
	}

	// build verticies.
	m_vertexCount = static_cast<s32>(verticies.size());

	// construct the vertex buffer.
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(RenderShader_Vertex_Generic) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = sizeof(RenderShader_Vertex_Generic);

	D3D11_SUBRESOURCE_DATA vertexData;
	ZeroMemory(&vertexData, sizeof(D3D11_SUBRESOURCE_DATA));
	vertexData.pSysMem = verticies.data();
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	assert(m_pVertexBuffer == nullptr);
	hr = pRenderer->GetDevice()->CreateBuffer(
		&vertexBufferDesc,
		&vertexData,
		&m_pVertexBuffer
	);
	assert(hr == S_OK);

	// build indicies
	assert(indicies.size() < 0x10000);
	m_indexCount = static_cast<s32>(indicies.size());

	// construct the index buffer.
	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(u16) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = sizeof(u16);

	D3D11_SUBRESOURCE_DATA indexData;
	ZeroMemory(&indexData, sizeof(D3D11_SUBRESOURCE_DATA));
	indexData.pSysMem = indicies.data();
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	assert(m_pIndexBuffer == nullptr);
	hr = pRenderer->GetDevice()->CreateBuffer(
		&indexBufferDesc,
		&indexData,
		&m_pIndexBuffer
	);
	assert(hr == S_OK);

	// init constant buffer.
	__InitVSConstantBuffers();
}

void RenderModel::__SetBoneTextureData(RenderModel_DAE_ParseContext& parseContext, f32* pBoneTextureData, const IVector2& boneTextureSize, const RenderModel_Anim& anim)
{
	const s32 xBase = 1 + anim.m_animIndex * 8;

	*(pBoneTextureData + 0 * boneTextureSize.x + xBase + 0) = static_cast<f32>(anim.m_animID);
	*(pBoneTextureData + 0 * boneTextureSize.x + xBase + 1) = static_cast<f32>(anim.m_animTime);
	*(pBoneTextureData + 0 * boneTextureSize.x + xBase + 2) = 0.f;
	*(pBoneTextureData + 0 * boneTextureSize.x + xBase + 3) = 0.f;
	*(pBoneTextureData + 0 * boneTextureSize.x + xBase + 4) = 0.f;
	*(pBoneTextureData + 0 * boneTextureSize.x + xBase + 5) = 0.f;
	*(pBoneTextureData + 0 * boneTextureSize.x + xBase + 6) = 0.f;
	*(pBoneTextureData + 0 * boneTextureSize.x + xBase + 7) = 0.f;

	for (std::vector<RenderModel_DAE_Mesh>::iterator itMesh = parseContext.m_meshes.begin(); itMesh != parseContext.m_meshes.end(); ++itMesh)
	{
		RenderModel_DAE_Mesh& mesh = *itMesh;

		for (std::vector<RenderModel_DAE_SkinJoint>::iterator itJoint = mesh.m_skinJoints.begin(); itJoint != mesh.m_skinJoints.end(); ++itJoint)
		{
			RenderModel_DAE_SkinJoint& joint = *itJoint;
			const Matrix4 matrix1 = __ComputeJointModelTransform(parseContext, joint, anim.m_animID);
			const Matrix4 matrix2 = Matrix4::MultiplyAB(matrix1, joint.m_invBindMatrix);
			const s32 yBase = 1 + joint.m_index * 4;

			*(pBoneTextureData + ((yBase + 0) * boneTextureSize.x) + xBase + 0) = matrix2.m[0][0];
			*(pBoneTextureData + ((yBase + 0) * boneTextureSize.x) + xBase + 1) = matrix2.m[1][0];
			*(pBoneTextureData + ((yBase + 0) * boneTextureSize.x) + xBase + 2) = matrix2.m[2][0];
			*(pBoneTextureData + ((yBase + 0) * boneTextureSize.x) + xBase + 3) = matrix2.m[3][0];

			*(pBoneTextureData + ((yBase + 1) * boneTextureSize.x) + xBase + 0) = matrix2.m[0][1];
			*(pBoneTextureData + ((yBase + 1) * boneTextureSize.x) + xBase + 1) = matrix2.m[1][1];
			*(pBoneTextureData + ((yBase + 1) * boneTextureSize.x) + xBase + 2) = matrix2.m[2][1];
			*(pBoneTextureData + ((yBase + 1) * boneTextureSize.x) + xBase + 3) = matrix2.m[3][1];

			*(pBoneTextureData + ((yBase + 2) * boneTextureSize.x) + xBase + 0) = matrix2.m[0][2];
			*(pBoneTextureData + ((yBase + 2) * boneTextureSize.x) + xBase + 1) = matrix2.m[1][2];
			*(pBoneTextureData + ((yBase + 2) * boneTextureSize.x) + xBase + 2) = matrix2.m[2][2];
			*(pBoneTextureData + ((yBase + 2) * boneTextureSize.x) + xBase + 3) = matrix2.m[3][2];

			*(pBoneTextureData + ((yBase + 3) * boneTextureSize.x) + xBase + 0) = matrix2.m[0][3];
			*(pBoneTextureData + ((yBase + 3) * boneTextureSize.x) + xBase + 1) = matrix2.m[1][3];
			*(pBoneTextureData + ((yBase + 3) * boneTextureSize.x) + xBase + 2) = matrix2.m[2][3];
			*(pBoneTextureData + ((yBase + 3) * boneTextureSize.x) + xBase + 3) = matrix2.m[3][3];

			const Matrix4 matrixNormal1 = Matrix4::ExtractRotation(matrix2);
			DirectX::XMMATRIX matrixNormal2;
			Matrix4ToXMMATRIX(matrixNormal1, matrixNormal2);
			DirectX::XMMATRIX matrixNormal3 = DirectX::XMMatrixInverse(nullptr, matrixNormal2);

			*(pBoneTextureData + ((yBase + 0) * boneTextureSize.x) + xBase + 4) = matrixNormal3.r[0].m128_f32[0];
			*(pBoneTextureData + ((yBase + 0) * boneTextureSize.x) + xBase + 5) = matrixNormal3.r[0].m128_f32[1];
			*(pBoneTextureData + ((yBase + 0) * boneTextureSize.x) + xBase + 6) = matrixNormal3.r[0].m128_f32[2];
			*(pBoneTextureData + ((yBase + 0) * boneTextureSize.x) + xBase + 7) = matrixNormal3.r[0].m128_f32[3];

			*(pBoneTextureData + ((yBase + 1) * boneTextureSize.x) + xBase + 4) = matrixNormal3.r[1].m128_f32[0];
			*(pBoneTextureData + ((yBase + 1) * boneTextureSize.x) + xBase + 5) = matrixNormal3.r[1].m128_f32[1];
			*(pBoneTextureData + ((yBase + 1) * boneTextureSize.x) + xBase + 6) = matrixNormal3.r[1].m128_f32[2];
			*(pBoneTextureData + ((yBase + 1) * boneTextureSize.x) + xBase + 7) = matrixNormal3.r[1].m128_f32[3];

			*(pBoneTextureData + ((yBase + 2) * boneTextureSize.x) + xBase + 4) = matrixNormal3.r[2].m128_f32[0];
			*(pBoneTextureData + ((yBase + 2) * boneTextureSize.x) + xBase + 5) = matrixNormal3.r[2].m128_f32[1];
			*(pBoneTextureData + ((yBase + 2) * boneTextureSize.x) + xBase + 6) = matrixNormal3.r[2].m128_f32[2];
			*(pBoneTextureData + ((yBase + 2) * boneTextureSize.x) + xBase + 7) = matrixNormal3.r[2].m128_f32[3];

			*(pBoneTextureData + ((yBase + 3) * boneTextureSize.x) + xBase + 4) = matrixNormal3.r[3].m128_f32[0];
			*(pBoneTextureData + ((yBase + 3) * boneTextureSize.x) + xBase + 5) = matrixNormal3.r[3].m128_f32[1];
			*(pBoneTextureData + ((yBase + 3) * boneTextureSize.x) + xBase + 6) = matrixNormal3.r[3].m128_f32[2];
			*(pBoneTextureData + ((yBase + 3) * boneTextureSize.x) + xBase + 7) = matrixNormal3.r[3].m128_f32[3];
		}
	}
}

RenderModel_DAE_Anim_Transform* RenderModel::__LookupAnimTransform(RenderModel_DAE_ParseContext& parseContext, const char* jointName, s32 animID)
{
	if (animID < 0)
		return nullptr;

	RenderModel_DAE_Anim* pAnim = nullptr;
	for (std::vector<RenderModel_DAE_Anim>::iterator it = parseContext.m_anims.begin(); it != parseContext.m_anims.end(); ++it)
	{
		RenderModel_DAE_Anim& anim = *it;
		if (anim.m_jointName == jointName)
		{
			pAnim = &anim;
		}
	}
	if (!pAnim)
		return nullptr;

	RenderModel_DAE_Anim_Transform* pBestTransform = nullptr;
	for (s32 i = 0; i < pAnim->m_transforms.size(); ++i)
	{
		RenderModel_DAE_Anim_Transform& transform = pAnim->m_transforms[i];
		if (transform.m_animID <= animID)
		{
			pBestTransform = &transform;
		}
	}

	return pBestTransform;
}

Matrix4 RenderModel::__ComputeJointModelTransform(RenderModel_DAE_ParseContext& parseContext, RenderModel_DAE_SkinJoint& joint, s32 animID)
{
	// is the result cached?
	if (animID < 0)
	{
		if (joint.m_isModelMatrixComputed)
			return joint.m_modelMatrix;
	}
	else if (!joint.m_animModelMatrix.empty())
	{
		RenderModel_DAE_SkinJoint_Anim_Transform* pAnimTransform = joint.GetTransformByAnimID(animID);
		assert(pAnimTransform);
		if (pAnimTransform->m_isModelMatrixComputed)
			return pAnimTransform->m_modelMatrix;
	}

	// compute the result.
	Matrix4 result;

	// compute parent transform.
	Matrix4 parentJointModelTransform;
	if (joint.m_parent >= 0)
	{
		RenderModel_DAE_SkinJoint& jointParent = joint.m_pMesh->m_skinJoints[joint.m_parent];
		parentJointModelTransform = __ComputeJointModelTransform(parseContext, jointParent, animID);
	}
	else
	{
		parentJointModelTransform = parseContext.m_baseJointMatrix;
	}

	RenderModel_DAE_Anim_Transform* pAnimTransform = __LookupAnimTransform(parseContext, joint.m_name.c_str(), animID);
	if (pAnimTransform)
	{
		result = Matrix4::MultiplyAB(parentJointModelTransform, pAnimTransform->m_jointMatrix);
	}
	else
	{
		result = Matrix4::MultiplyAB(parentJointModelTransform, joint.m_jointMatrix);
	}

	// cache result.
	if (animID < 0)
	{
		joint.m_isModelMatrixComputed = true;
		joint.m_modelMatrix = result;
	}
	else
	{
		if (joint.m_animModelMatrix.empty())
		{
			joint.m_animModelMatrix.reserve(parseContext.m_animIDMap.size());
			for (std::map<f32, s32>::iterator it = parseContext.m_animIDMap.begin(); it != parseContext.m_animIDMap.end(); ++it)
			{
				RenderModel_DAE_SkinJoint_Anim_Transform node;
				node.m_animID = it->second;
				joint.m_animModelMatrix.push_back(node);
			}
		}

		RenderModel_DAE_SkinJoint_Anim_Transform* pAnimTransform = joint.GetTransformByAnimID(animID);
		pAnimTransform->m_isModelMatrixComputed = true;
		pAnimTransform->m_modelMatrix = result;
	}

	return result;
}

XML_Parser_Result RenderModel::__ParseDAERead(TB8::File* f, u8* pBuf, u32* pSize)
{
	*pSize = f->Read(pBuf, *pSize);
	return (*pSize > 0) ? XML_Parser_Result_Success : XML_Parser_Result_EOF;
}

void RenderModel::__ParseDAEStartElement(void *_ctx, const char* name, const char** _atts)
{
	RenderModel_DAE_ParseContext* ctx = reinterpret_cast<RenderModel_DAE_ParseContext*>(_ctx);
	ctx->m_tagStack.push_back(std::string(reinterpret_cast<const char*>(name)));

	// id stack.
	{
		for (const char** atts = _atts; atts && *atts; atts += 2)
		{
			const char* attrib = *(atts + 0);
			const char* value = *(atts + 1);
			if (_strcmpi(attrib, "id") == 0)
			{
				ctx->m_idStack.push_back(std::make_pair(std::string(reinterpret_cast<const char*>(name)), std::string(value)));
			}
		}
	}

	// is this the model we were looking for?
	if ((ctx->m_tagStack.size() == 3)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "asset") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "up_axis") == 0))
	{
		ctx->m_fnParseChars = &__ParseChars_SingleString;
		ctx->m_fnParseBuffer = &__ParseBuffer_SingleString;
		ctx->m_fnParseDataSet = &__ParseDataSet_UpAxis;
	}

	// textures
	if ((ctx->m_tagStack.size() == 4)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_images") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "image") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "init_from") == 0))
	{
		ctx->m_fnParseChars = &__ParseChars_SingleString;
		ctx->m_fnParseBuffer = &__ParseBuffer_SingleString;
		ctx->m_fnParseDataSet = &__ParseDataSet_TextureFileName;
	}

	// effects
	if ((ctx->m_tagStack.size() == 8)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_effects") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "effect") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "profile_COMMON") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "technique") == 0)
		&& (_stricmp(ctx->m_tagStack[7].c_str(), "color") == 0))
	{
		if (_stricmp(ctx->m_tagStack[6].c_str(), "emission") == 0)
		{
			ctx->m_fnParseChars = &__ParseChars_Numbers;
			ctx->m_fnParseBuffer = &__ParseBuffer_F32;
			ctx->m_fnParseDataSet = &__ParseDataSet_EffectEmission;
		}
		else if (_stricmp(ctx->m_tagStack[6].c_str(), "diffuse") == 0)
		{
			ctx->m_fnParseChars = &__ParseChars_Numbers;
			ctx->m_fnParseBuffer = &__ParseBuffer_F32;
			ctx->m_fnParseDataSet = &__ParseDataSet_EffectDiffuse;
		}
		else if (_stricmp(ctx->m_tagStack[6].c_str(), "specular") == 0)
		{
			ctx->m_fnParseChars = &__ParseChars_Numbers;
			ctx->m_fnParseBuffer = &__ParseBuffer_F32;
			ctx->m_fnParseDataSet = &__ParseDataSet_EffectSpecular;
		}
	}

	if ((ctx->m_tagStack.size() == 7)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_effects") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "effect") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "profile_COMMON") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "newparam") == 0)
		&& (_stricmp(ctx->m_tagStack[5].c_str(), "surface") == 0)
		&& (_stricmp(ctx->m_tagStack[6].c_str(), "init_from") == 0))
	{
		ctx->m_fnParseChars = &__ParseChars_SingleString;
		ctx->m_fnParseBuffer = &__ParseBuffer_SingleString;
		ctx->m_fnParseDataSet = &__ParseDataSet_EffectTexture;
	}

	// materials
	if ((ctx->m_tagStack.size() == 4)
		&& !ctx->m_idStack.empty()
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_materials") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "material") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "instance_effect") == 0))
	{
		for (const char** atts = _atts; atts && *atts; atts += 2)
		{
			const char* attrib = *(atts + 0);
			const char* value = *(atts + 1);
			if (_strcmpi(attrib, "url") == 0)
			{
				RenderModel_DAE_Material data;
				data.m_id = ctx->m_idStack.back().second;
				data.m_effectIndex = -1;

				std::string effectID = (*value == '#') ? value + 1 : value;

				s32 index = 0;
				for (std::vector<RenderModel_DAE_Effect>::iterator it = ctx->m_effects.begin(); it != ctx->m_effects.end(); ++it, ++index)
				{
					if (it->m_id == effectID)
					{
						data.m_effectIndex = index;
						break;
					}
				}

				if (!data.m_id.empty() && (data.m_effectIndex != -1))
				{
					ctx->m_materials.push_back(data);
				}
			}
		}
	}

	// is this the model we were looking for?
	if ((ctx->m_tagStack.size() == 3)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_geometries") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "geometry") == 0))
	{
		for (const char** atts = _atts; atts && *atts; atts += 2)
		{
			const char* attrib = *(atts + 0);
			const char* value = *(atts + 1);
			if (_strcmpi(attrib, "name") == 0)
			{
				for (std::vector<RenderModel_DAE_Mesh>::iterator itMesh = ctx->m_meshes.begin(); itMesh != ctx->m_meshes.end(); ++itMesh)
				{
					RenderModel_DAE_Mesh& mesh = *itMesh;
					if (_strcmpi(value, mesh.m_name.c_str()) == 0)
					{
						mesh.m_id = ctx->m_idStack.back().second;
						ctx->m_pMesh = &mesh;
						break;
					}
				}
			}
		}
	}

	// vertex, normal, map data.
	if (ctx->m_pMesh
		&& (ctx->m_tagStack.size() == 6)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_geometries") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "geometry") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "mesh") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "source") == 0)
		&& (_stricmp(ctx->m_tagStack[5].c_str(), "float_array") == 0))
	{
		const std::string& lastID = ctx->m_idStack.back().second;
		if (lastID.find("positions-array") != std::string::npos)
		{
			ctx->m_fnParseChars = &__ParseChars_Numbers;
			ctx->m_fnParseBuffer = &__ParseBuffer_F32;
			ctx->m_fnParseDataSet = &__ParseDataSet_MeshVertex;
		}
		else if (lastID.find("normals-array") != std::string::npos)
		{
			ctx->m_fnParseChars = &__ParseChars_Numbers;
			ctx->m_fnParseBuffer = &__ParseBuffer_F32;
			ctx->m_fnParseDataSet = &__ParseDataSet_MeshNormal;
		}
		else if (lastID.find("map-0-array") != std::string::npos)
		{
			ctx->m_fnParseChars = &__ParseChars_Numbers;
			ctx->m_fnParseBuffer = &__ParseBuffer_F32;
			ctx->m_fnParseDataSet = &__ParseDataSet_MeshMap;
		}
	}

	// triangles.
	if (ctx->m_pMesh
		&& (ctx->m_tagStack.size() == 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_geometries") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "geometry") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "mesh") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "triangles") == 0))
	{
		ctx->m_materialIndex = -1;
		ctx->m_inputSemantic.clear();
		for (const char** atts = _atts; atts && *atts; atts += 2)
		{
			const char* attrib = *(atts + 0);
			const char* value = *(atts + 1);
			if (_strcmpi(attrib, "material") == 0)
			{
				s32 index = 0;
				for (std::vector<RenderModel_DAE_Material>::iterator it = ctx->m_materials.begin(); it != ctx->m_materials.end(); ++it, ++index)
				{
					if (it->m_id == value)
					{
						ctx->m_materialIndex = index;
						break;
					}
				}
			}
		}
	}

	// triangle input.
	if (ctx->m_pMesh
		&& (ctx->m_materialIndex >= 0)
		&& (ctx->m_tagStack.size() == 6)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_geometries") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "geometry") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "mesh") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "triangles") == 0)
		&& (_stricmp(ctx->m_tagStack[5].c_str(), "input") == 0))
	{
		for (const char** atts = _atts; atts && *atts; atts += 2)
		{
			const char* attrib = *(atts + 0);
			const char* value = *(atts + 1);
			if (_strcmpi(attrib, "semantic") == 0)
			{
				if (_strcmpi(value, "vertex") == 0)
				{
					ctx->m_inputSemantic.push_back(RenderModel_DAE_InputSemantic_Vertex);
				}
				else if (_strcmpi(value, "normal") == 0)
				{
					ctx->m_inputSemantic.push_back(RenderModel_DAE_InputSemantic_Normal);
				}
				else if (_strcmpi(value, "texcoord") == 0)
				{
					ctx->m_inputSemantic.push_back(RenderModel_DAE_InputSemantic_TexCoord);
				}
			}
		}
	}

	// triangle data.
	if (ctx->m_pMesh
		&& (ctx->m_materialIndex >= 0)
		&& (ctx->m_tagStack.size() == 6)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_geometries") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "geometry") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "mesh") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "triangles") == 0)
		&& (_stricmp(ctx->m_tagStack[5].c_str(), "p") == 0))
	{
		ctx->m_fnParseChars = &__ParseChars_Numbers;
		ctx->m_fnParseBuffer = &__ParseBuffer_S32;
		ctx->m_fnParseDataSet = &__ParseDataSet_MeshTriangle;
	}

	// animations.
	if ((ctx->m_tagStack.size() >= 5)
		&& !ctx->m_idStack.empty()
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_animations") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 3].c_str(), "animation") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 2].c_str(), "source") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 1].c_str(), "float_array") == 0))
	{
		std::string& id = ctx->m_idStack.back().second;
		if (id.find("pose_matrix-input-array") != std::string::npos)
		{
			ctx->m_fnParseChars = &__ParseChars_Numbers;
			ctx->m_fnParseBuffer = &__ParseBuffer_F32;
			ctx->m_fnParseDataSet = &__ParseDataSet_AnimIDs;
			ctx->m_tempAnimIDs.clear();
		}
		else if (id.find("pose_matrix-output-array") != std::string::npos)
		{
			// find joint.
			RenderModel_DAE_SkinJoint* pJoint = nullptr;
			for (std::vector<RenderModel_DAE_Mesh>::iterator itMesh = ctx->m_meshes.begin(); itMesh != ctx->m_meshes.end() && !pJoint; ++itMesh)
			{
				RenderModel_DAE_Mesh& mesh = *itMesh;
				for (std::vector<RenderModel_DAE_SkinJoint>::iterator itJoint = mesh.m_skinJoints.begin(); itJoint != mesh.m_skinJoints.end() && !pJoint; ++itJoint)
				{
					RenderModel_DAE_SkinJoint& joint = *itJoint;
					if (id.find(joint.m_name.c_str()) == std::string::npos)
						continue;

					pJoint = &joint;
				}
			}

			if (pJoint)
			{
				ctx->m_anims.emplace_back();
				RenderModel_DAE_Anim& node = ctx->m_anims.back();
				node.m_jointName = pJoint->m_name;

				ctx->m_fnParseChars = &__ParseChars_Numbers;
				ctx->m_fnParseBuffer = &__ParseBuffer_F32;
				ctx->m_fnParseDataSet = &__ParseDataSet_AnimMatrix;
				ctx->m_animIndex = 0;
			}
		}
	}

	// skin
	if ((ctx->m_tagStack.size() == 4)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_controllers") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "controller") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "skin") == 0))
	{
		for (const char** atts = _atts; atts && *atts; atts += 2)
		{
			const char* attrib = *(atts + 0);
			const char* value = *(atts + 1);
			if (_strcmpi(attrib, "source") == 0)
			{
				for (std::vector<RenderModel_DAE_Mesh>::iterator it = ctx->m_meshes.begin(); it != ctx->m_meshes.end(); ++it)
				{
					RenderModel_DAE_Mesh& mesh = *it;
					if (_strcmpi(value + 1, mesh.m_id.c_str()) == 0)
					{
						ctx->m_pMesh = &mesh;
						ctx->m_pMesh->m_skinName = ctx->m_idStack[ctx->m_idStack.size() - 1].second;
						break;
					}
				}
			}
		}
	}

	// bind shape matrix.
	if (ctx->m_pMesh
		&& (ctx->m_tagStack.size() == 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_controllers") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "controller") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "skin") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "bind_shape_matrix") == 0))
	{
		ctx->m_fnParseChars = &__ParseChars_Numbers;
		ctx->m_fnParseBuffer = &__ParseBuffer_F32;
		ctx->m_fnParseDataSet = &__ParseDataSet_BindShapeMatrix;
	}

	// joint names.
	if (ctx->m_pMesh
		&& (ctx->m_tagStack.size() == 6)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_controllers") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "controller") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "skin") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "source") == 0)
		&& (_stricmp(ctx->m_tagStack[5].c_str(), "Name_array") == 0)
		&& (ctx->m_idStack.back().second.find("skin-joints-array") != std::string::npos))
	{
		ctx->m_fnParseChars = &__ParseChars_MultiString;
		ctx->m_fnParseBuffer = &__ParseBuffer_MultiString;
		ctx->m_fnParseDataSet = &__ParseDataSet_JointName;
	}

	// skin inv bind matrix
	if (ctx->m_pMesh
		&& (ctx->m_tagStack.size() == 6)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_controllers") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "controller") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "skin") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "source") == 0)
		&& (_stricmp(ctx->m_tagStack[5].c_str(), "float_array") == 0)
		&& (ctx->m_idStack.back().second.find("skin-bind_poses-array") != std::string::npos))
	{
		ctx->m_fnParseChars = &__ParseChars_Numbers;
		ctx->m_fnParseBuffer = &__ParseBuffer_F32;
		ctx->m_fnParseDataSet = &__ParseDataSet_JointInvBindMatrix;
		ctx->m_jointIndex = 0;
	}

	// skin weights
	if (ctx->m_pMesh
		&& (ctx->m_tagStack.size() == 6)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_controllers") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "controller") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "skin") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "source") == 0)
		&& (_stricmp(ctx->m_tagStack[5].c_str(), "float_array") == 0)
		&& (ctx->m_idStack.back().second.find("skin-weights-array") != std::string::npos))
	{
		ctx->m_fnParseChars = &__ParseChars_Numbers;
		ctx->m_fnParseBuffer = &__ParseBuffer_F32;
		ctx->m_fnParseDataSet = &__ParseDataSet_SkinWeight;
	}

	// vertex weights
	if (ctx->m_pMesh
		&& (ctx->m_tagStack.size() == 6)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_controllers") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "controller") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "skin") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "vertex_weights") == 0))
	{
		if (_stricmp(ctx->m_tagStack[5].c_str(), "vcount") == 0)
		{
			ctx->m_fnParseChars = &__ParseChars_Numbers;
			ctx->m_fnParseBuffer = &__ParseBuffer_S32;
			ctx->m_fnParseDataSet = &__ParseDataSet_VertexWeightCounts;
		}
		else if (_stricmp(ctx->m_tagStack[5].c_str(), "v") == 0)
		{
			ctx->m_fnParseChars = &__ParseChars_Numbers;
			ctx->m_fnParseBuffer = &__ParseBuffer_S32;
			ctx->m_fnParseDataSet = &__ParseDataSet_VertexWeights;
			ctx->m_vertexIndex = 0;
		}
	}

	// armature joint node.
	if ((ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack.back().c_str(), "node") == 0)
		&& (ctx->m_idStack.size() >= 2)
		&& (ctx->m_idStack.back().second.find("Armature") != std::string::npos))
	{
		for (const char** atts = _atts; atts && *atts; atts += 2)
		{
			const char* attrib = *(atts + 0);
			const char* value = *(atts + 1);
			if (_strcmpi(attrib, "sid") == 0)
			{
				for (std::vector<RenderModel_DAE_Mesh>::iterator itMesh = ctx->m_meshes.begin(); itMesh != ctx->m_meshes.end(); ++itMesh)
				{
					RenderModel_DAE_Mesh& mesh = *itMesh;
					for (std::vector<RenderModel_DAE_SkinJoint>::iterator itJoint = mesh.m_skinJoints.begin(); itJoint != mesh.m_skinJoints.end(); ++itJoint)
					{
						RenderModel_DAE_SkinJoint& joint = *itJoint;
						if (_stricmp(joint.m_name.c_str(), value) == 0)
						{
							if (!ctx->m_skinJointStack.empty())
							{
								const s32 jointIndex = joint.m_index;
								RenderModel_DAE_SkinJoint* jointParent = ctx->m_skinJointStack.back();
								const s32 parentIndex = jointParent->m_index;
								assert(parentIndex != joint.m_index);
								assert(joint.m_parent == -1);
								joint.m_parent = parentIndex;
								jointParent->m_children.push_back(joint.m_index);
							}

							ctx->m_skinJointStack.push_back(&joint);
							break;
						}
					}
				}
			}
		}
	}

#if 0
	// armature joint matrix
	if ((ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 2].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 1].c_str(), "matrix") == 0)
		&& !ctx->m_skinJointStack.empty())
	{
		ctx->m_fnParseChars = &__ParseChars_Numbers;
		ctx->m_fnParseBuffer = &__ParseBuffer_F32;
		ctx->m_fnParseDataSet = &__ParseDataSet_JointMatrix;
	}

	// named locations.
	if ((ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "translate") == 0)
		&& (ctx->m_idStack.back().second.find("POS_") != std::string::npos))
	{
		ctx->m_fnParseChars = &__ParseChars_Numbers;
		ctx->m_fnParseBuffer = &__ParseBuffer_F32;
		ctx->m_fnParseDataSet = &__ParseDataSet_NamedPositionTranslate;
	}
	if ((ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "matrix") == 0)
		&& (ctx->m_idStack.back().second.find("POS_") != std::string::npos))
	{
		ctx->m_fnParseChars = &__ParseChars_Numbers;
		ctx->m_fnParseBuffer = &__ParseBuffer_F32;
		ctx->m_fnParseDataSet = &__ParseDataSet_NamedPositionMatrix;
	}
#endif

	// instance geometry transform matrix stack.
	if ((ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 1].c_str(), "matrix") == 0))
	{
		ctx->m_fnParseChars = &__ParseChars_Numbers;
		ctx->m_fnParseBuffer = &__ParseBuffer_F32;
		ctx->m_fnParseDataSet = &__ParseDataSet_PositionTransformStack_Matrix;
	}

	if ((ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 1].c_str(), "translate") == 0))
	{
		ctx->m_fnParseChars = &__ParseChars_Numbers;
		ctx->m_fnParseBuffer = &__ParseBuffer_F32;
		ctx->m_fnParseDataSet = &__ParseDataSet_PositionTransformStack_Translate;
	}

	// instance geometry.
	if ((ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "instance_geometry") == 0))
	{
		for (const char** atts = _atts; atts && *atts; atts += 2)
		{
			const char* attrib = *(atts + 0);
			const char* value = *(atts + 1);
			if (_strcmpi(attrib, "url") == 0)
			{
				for (std::vector<RenderModel_DAE_Mesh>::iterator it = ctx->m_meshes.begin(); it != ctx->m_meshes.end(); ++it)
				{
					RenderModel_DAE_Mesh& mesh = *it;
					if (_strcmpi(value + 1, mesh.m_id.c_str()) == 0)
					{
						ctx->m_pMesh = &mesh;
						break;
					}
				}
			}
		}
	}

	// instance controller.
	if ((ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "instance_controller") == 0))
	{
		for (const char** atts = _atts; atts && *atts; atts += 2)
		{
			const char* attrib = *(atts + 0);
			const char* value = *(atts + 1);
			if (_strcmpi(attrib, "url") == 0)
			{
				for (std::vector<RenderModel_DAE_Mesh>::iterator it = ctx->m_meshes.begin(); it != ctx->m_meshes.end(); ++it)
				{
					RenderModel_DAE_Mesh& mesh = *it;
					if (_strcmpi(value + 1, mesh.m_skinName.c_str()) == 0)
					{
						ctx->m_pMesh = &mesh;
						break;
					}
				}
			}
		}
	}
}

void RenderModel::__ParseDAECharacters(void *_ctx, const char* value, int len)
{
	RenderModel_DAE_ParseContext* ctx = reinterpret_cast<RenderModel_DAE_ParseContext*>(_ctx);

	if (ctx->m_fnParseChars)
	{
		// parse out data
		for (; len > 0; value++, len--)
		{
			const char v = *value;

			if ((*(ctx->m_fnParseChars))(ctx, v))
			{
				ctx->m_buffer.push_back(v);
			}
			else
			{
				(*(ctx->m_fnParseBuffer))(ctx);
			}
		}
	}
}

bool RenderModel::__ParseChars_SingleString(RenderModel_DAE_ParseContext* ctx, char v)
{
	return true;
}

bool RenderModel::__ParseChars_MultiString(RenderModel_DAE_ParseContext* ctx, char v)
{
	return v != 0x20;
}

bool RenderModel::__ParseChars_Numbers(RenderModel_DAE_ParseContext* ctx, char v)
{
	return isdigit(v) || v == '-' || v == '.' || v == 'e';
}

void RenderModel::__ParseBuffer_F32(RenderModel_DAE_ParseContext* ctx)
{
	if (!ctx->m_buffer.empty())
	{
		ctx->m_f32.push_back(std::stof(ctx->m_buffer));
		ctx->m_buffer.clear();
		(*(ctx->m_fnParseDataSet))(ctx);
	}
}

void RenderModel::__ParseBuffer_S32(RenderModel_DAE_ParseContext* ctx)
{
	if (!ctx->m_buffer.empty())
	{
		ctx->m_s32.push_back(std::stol(ctx->m_buffer));
		ctx->m_buffer.clear();
		(*(ctx->m_fnParseDataSet))(ctx);
	}
}

void RenderModel::__ParseBuffer_SingleString(RenderModel_DAE_ParseContext* ctx)
{
	if (!ctx->m_buffer.empty())
	{
		(*(ctx->m_fnParseDataSet))(ctx);
		ctx->m_buffer.clear();
	}
}

void RenderModel::__ParseBuffer_MultiString(RenderModel_DAE_ParseContext* ctx)
{
	if (!ctx->m_buffer.empty())
	{
		(*(ctx->m_fnParseDataSet))(ctx);
		ctx->m_buffer.clear();
	}
}

void RenderModel::__ParseDataSet_MeshVertex(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() == 3)
	{
		RenderModel_DAE_Vertex vertex;
		vertex.m_pos.x = ctx->m_f32[0];
		vertex.m_pos.y = ctx->m_f32[1];
		vertex.m_pos.z = ctx->m_f32[2];

		// update min/max.
		ctx->m_pMesh->m_bounds.AddVector(vertex.m_pos);
		ctx->m_pMesh->m_meshVerticies.push_back(vertex);
		ctx->m_f32.clear();
	}
}

void RenderModel::__ParseDataSet_MeshNormal(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() == 3)
	{
		Vector3 data;
		data.x = ctx->m_f32[0];
		data.y = ctx->m_f32[1];
		data.z = ctx->m_f32[2];
		ctx->m_pMesh->m_meshNormals.push_back(data);
		ctx->m_f32.clear();
	}
}

void RenderModel::__ParseDataSet_MeshMap(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() == 2)
	{
		Vector2 data;
		data.x = ctx->m_f32[0];
		data.y = ctx->m_f32[1];
		ctx->m_pMesh->m_meshMap.push_back(data);
		ctx->m_f32.clear();
	}
}

void RenderModel::__ParseDataSet_MeshTriangle(RenderModel_DAE_ParseContext* ctx)
{
	const s32 valuesPerVertex = static_cast<s32>(ctx->m_inputSemantic.size());
	const s32 desiredValCount = valuesPerVertex * 3;
	if (ctx->m_s32.size() < desiredValCount)
		return;

	RenderModel_DAE_Triangle triangle;

	for (s32 iVertex = 0; iVertex < 3; ++iVertex)
	{
		for (s32 iValue = 0; iValue < valuesPerVertex; ++iValue)
		{
			switch (ctx->m_inputSemantic[iValue])
			{
			case RenderModel_DAE_InputSemantic_Vertex:
				triangle.m_vertex[iVertex].m_vertexIndex = ctx->m_s32[(iVertex * valuesPerVertex) + iValue];
				break;
			case RenderModel_DAE_InputSemantic_Normal:
				triangle.m_vertex[iVertex].m_normalIndex = ctx->m_s32[(iVertex * valuesPerVertex) + iValue];
				break;
			case RenderModel_DAE_InputSemantic_TexCoord:
				triangle.m_vertex[iVertex].m_mapIndex = ctx->m_s32[(iVertex * valuesPerVertex) + iValue];
				break;
			default:
				assert(!"unhandled input semantic");
				break;
			}
		}
	}

	triangle.m_materialIndex = ctx->m_materialIndex;

	ctx->m_pModel->__CheckVertexOrder(*ctx, *(ctx->m_pMesh), triangle);

	ctx->m_pMesh->m_meshTriangles.push_back(triangle);

	ctx->m_s32.clear();
}

void RenderModel::__CheckVertexOrder(RenderModel_DAE_ParseContext& parseContext, RenderModel_DAE_Mesh& mesh, RenderModel_DAE_Triangle& triangle)
{
	// ensure the triangles are going clockwise around the provided normal.
	const Vector3& v1a = mesh.m_meshVerticies[triangle.m_vertex[0].m_vertexIndex].m_pos;
	const Vector3& v2a = mesh.m_meshVerticies[triangle.m_vertex[1].m_vertexIndex].m_pos;
	const Vector3& v3a = mesh.m_meshVerticies[triangle.m_vertex[2].m_vertexIndex].m_pos;
	assert(triangle.m_vertex[0].m_normalIndex == triangle.m_vertex[1].m_normalIndex);
	assert(triangle.m_vertex[0].m_normalIndex == triangle.m_vertex[2].m_normalIndex);
	const Vector3& n1a = mesh.m_meshNormals[triangle.m_vertex[0].m_normalIndex];

	// translate to directX coordinates
	const Vector3 v1b = Matrix4::MultiplyVector(v1a, parseContext.m_coordTranslate);
	const Vector3 v2b = Matrix4::MultiplyVector(v2a, parseContext.m_coordTranslate);
	const Vector3 v3b = Matrix4::MultiplyVector(v3a, parseContext.m_coordTranslate);
	const Vector3 n1b = Matrix4::MultiplyVector(n1a, parseContext.m_coordTranslate);

	const Vector3 u(v2b.x - v1b.x, v2b.y - v1b.y, v2b.z - v1b.z);
	const Vector3 v(v3b.x - v1b.x, v3b.y - v1b.y, v3b.z - v1b.z);
	const Vector3 cross((u.y*v.z - u.z*v.y), (u.z*v.x - u.x*v.z), (u.x*v.y - u.y*v.x));

	const f32 dot = (cross.x * n1b.x) + (cross.y * n1b.y) + (cross.z * n1b.z);
	if (dot < 0.f)
	{
		const RenderModel_DAE_Triangle_Vertex temp = triangle.m_vertex[2];
		triangle.m_vertex[2] = triangle.m_vertex[1];
		triangle.m_vertex[1] = temp;
	}
}

void RenderModel::__ParseDataSet_TextureFileName(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_buffer.empty())
		return;

	std::string imageFilePath = ctx->m_assetPath;
	TB8::File::AppendToPath(imageFilePath, ctx->m_buffer.c_str());

	RenderModel_DAE_Texture data;
	data.m_id = ctx->m_idStack.back().second;
	data.m_path = imageFilePath;
	data.m_index = static_cast<u32>(ctx->m_textures.size());

	ctx->m_textures.push_back(data);
	ctx->m_buffer.clear();
}

void RenderModel::__ParseDataSet_JointName(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_buffer.empty())
		return;

	RenderModel_DAE_SkinJoint data;
	data.m_name = ctx->m_buffer;
	data.m_index = static_cast<s32>(ctx->m_pMesh->m_skinJoints.size());
	data.m_pMesh = ctx->m_pMesh;
	ctx->m_pMesh->m_skinJoints.push_back(data);

	ctx->m_buffer.clear();
}

void RenderModel::__ParseDataSet_BindShapeMatrix(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() < 16)
		return;

	Matrix4 matrix1;
	matrix1.m[0][0] = ctx->m_f32[0];
	matrix1.m[1][0] = ctx->m_f32[1];
	matrix1.m[2][0] = ctx->m_f32[2];
	matrix1.m[3][0] = ctx->m_f32[3];
	matrix1.m[0][1] = ctx->m_f32[4];
	matrix1.m[1][1] = ctx->m_f32[5];
	matrix1.m[2][1] = ctx->m_f32[6];
	matrix1.m[3][1] = ctx->m_f32[7];
	matrix1.m[0][2] = ctx->m_f32[8];
	matrix1.m[1][2] = ctx->m_f32[9];
	matrix1.m[2][2] = ctx->m_f32[10];
	matrix1.m[3][2] = ctx->m_f32[11];
	matrix1.m[0][3] = ctx->m_f32[12];
	matrix1.m[1][3] = ctx->m_f32[13];
	matrix1.m[2][3] = ctx->m_f32[14];
	matrix1.m[3][3] = ctx->m_f32[15];

	ctx->m_pMesh->m_bindShapeMatrix = matrix1;

	ctx->m_f32.clear();
}

void RenderModel::__ParseDataSet_JointInvBindMatrix(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() < 16)
		return;

	Matrix4 matrix1;
	matrix1.m[0][0] = ctx->m_f32[0];
	matrix1.m[1][0] = ctx->m_f32[1];
	matrix1.m[2][0] = ctx->m_f32[2];
	matrix1.m[3][0] = ctx->m_f32[3];
	matrix1.m[0][1] = ctx->m_f32[4];
	matrix1.m[1][1] = ctx->m_f32[5];
	matrix1.m[2][1] = ctx->m_f32[6];
	matrix1.m[3][1] = ctx->m_f32[7];
	matrix1.m[0][2] = ctx->m_f32[8];
	matrix1.m[1][2] = ctx->m_f32[9];
	matrix1.m[2][2] = ctx->m_f32[10];
	matrix1.m[3][2] = ctx->m_f32[11];
	matrix1.m[0][3] = ctx->m_f32[12];
	matrix1.m[1][3] = ctx->m_f32[13];
	matrix1.m[2][3] = ctx->m_f32[14];
	matrix1.m[3][3] = ctx->m_f32[15];

	RenderModel_DAE_SkinJoint& joint = ctx->m_pMesh->m_skinJoints[ctx->m_jointIndex];
	joint.m_invBindMatrix = matrix1;

	ctx->m_f32.clear();
	ctx->m_jointIndex++;
}

void RenderModel::__ParseDataSet_SkinWeight(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() == 1)
	{
		RenderModel_DAE_SkinWeight data;
		data.m_weight = ctx->m_f32[0];
		ctx->m_pMesh->m_skinWeights.push_back(data);

		ctx->m_f32.clear();
	}
}

void RenderModel::__ParseDataSet_VertexWeightCounts(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_s32.size() == 1)
	{
		ctx->m_pMesh->m_vertexWeightCounts.push_back(ctx->m_s32[0]);

		ctx->m_s32.clear();
	}
}

void RenderModel::__ParseDataSet_VertexWeights(RenderModel_DAE_ParseContext* ctx)
{
	while ((ctx->m_vertexIndex < ctx->m_pMesh->m_vertexWeightCounts.size()) && (ctx->m_pMesh->m_vertexWeightCounts[ctx->m_vertexIndex] == 0))
	{
		ctx->m_vertexIndex++;
	}

	if (ctx->m_vertexIndex >= ctx->m_pMesh->m_vertexWeightCounts.size())
	{
		ctx->m_s32.clear();
		return;
	}

	const s32 index = ctx->m_vertexIndex;
	const s32 count = ctx->m_pMesh->m_vertexWeightCounts[index];

	if (ctx->m_s32.size() < (count * 2))
		return;

	RenderModel_DAE_Vertex& vertex = ctx->m_pMesh->m_meshVerticies[index];

	for (s32 i = 0; i < ctx->m_s32.size(); i += 2)
	{
		RenderModel_DAE_Vertex_SkinJointWeight data;
		data.m_jointIndex = ctx->m_s32[i + 0];
		const s32 weightIndex = ctx->m_s32[i + 1];
		data.m_weight = ctx->m_pMesh->m_skinWeights[weightIndex].m_weight;
		vertex.m_weights.push_back(data);
	}

	// sort such that most important weights come first.
	std::sort(vertex.m_weights.begin(), vertex.m_weights.end(), RenderModel_DAE_Vertex_SkinJointWeight::Sort);

	// if necessary, truncate the least important weights.
	if (vertex.m_weights.size() > 4)
	{
		const f32 total = vertex.m_weights[0].m_weight + vertex.m_weights[1].m_weight + vertex.m_weights[2].m_weight + vertex.m_weights[3].m_weight;
		vertex.m_weights[0].m_weight = vertex.m_weights[0].m_weight / total;
		vertex.m_weights[1].m_weight = vertex.m_weights[1].m_weight / total;
		vertex.m_weights[2].m_weight = vertex.m_weights[2].m_weight / total;
		vertex.m_weights[3].m_weight = vertex.m_weights[3].m_weight / total;
		vertex.m_weights.resize(4);
	}

	ctx->m_vertexIndex++;
	ctx->m_s32.clear();
}

void RenderModel::__ParseDataSet_JointMatrix(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() < 16)
		return;

	Matrix4 matrix1;
	matrix1.m[0][0] = ctx->m_f32[0];
	matrix1.m[1][0] = ctx->m_f32[1];
	matrix1.m[2][0] = ctx->m_f32[2];
	matrix1.m[3][0] = ctx->m_f32[3];
	matrix1.m[0][1] = ctx->m_f32[4];
	matrix1.m[1][1] = ctx->m_f32[5];
	matrix1.m[2][1] = ctx->m_f32[6];
	matrix1.m[3][1] = ctx->m_f32[7];
	matrix1.m[0][2] = ctx->m_f32[8];
	matrix1.m[1][2] = ctx->m_f32[9];
	matrix1.m[2][2] = ctx->m_f32[10];
	matrix1.m[3][2] = ctx->m_f32[11];
	matrix1.m[0][3] = ctx->m_f32[12];
	matrix1.m[1][3] = ctx->m_f32[13];
	matrix1.m[2][3] = ctx->m_f32[14];
	matrix1.m[3][3] = ctx->m_f32[15];

	RenderModel_DAE_SkinJoint* joint = ctx->m_skinJointStack.back();
	joint->m_jointMatrix = matrix1;

	ctx->m_f32.clear();
}

void RenderModel::__ParseDataSet_AnimIDs(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() < 1)
		return;

	s32 animID = 0;
	std::map<f32, s32>::iterator it = ctx->m_animIDMap.find(ctx->m_f32[0]);
	if (it != ctx->m_animIDMap.end())
	{
		animID = it->second;
	}
	else
	{
		animID = ++ctx->m_animIDGen;
		ctx->m_animIDMap.insert(std::make_pair(ctx->m_f32[0], animID));
	}

	ctx->m_tempAnimIDs.push_back(animID);
	ctx->m_f32.clear();
}

void RenderModel::__ParseDataSet_AnimMatrix(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() < 16)
		return;

	Matrix4 matrix1;
	matrix1.m[0][0] = ctx->m_f32[0];
	matrix1.m[1][0] = ctx->m_f32[1];
	matrix1.m[2][0] = ctx->m_f32[2];
	matrix1.m[3][0] = ctx->m_f32[3];
	matrix1.m[0][1] = ctx->m_f32[4];
	matrix1.m[1][1] = ctx->m_f32[5];
	matrix1.m[2][1] = ctx->m_f32[6];
	matrix1.m[3][1] = ctx->m_f32[7];
	matrix1.m[0][2] = ctx->m_f32[8];
	matrix1.m[1][2] = ctx->m_f32[9];
	matrix1.m[2][2] = ctx->m_f32[10];
	matrix1.m[3][2] = ctx->m_f32[11];
	matrix1.m[0][3] = ctx->m_f32[12];
	matrix1.m[1][3] = ctx->m_f32[13];
	matrix1.m[2][3] = ctx->m_f32[14];
	matrix1.m[3][3] = ctx->m_f32[15];

	RenderModel_DAE_Anim& node = ctx->m_anims.back();
	RenderModel_DAE_Anim_Transform data;
	assert(ctx->m_animIndex < ctx->m_tempAnimIDs.size());
	data.m_animID = ctx->m_tempAnimIDs[ctx->m_animIndex];
	data.m_jointMatrix = matrix1;
	node.m_transforms.push_back(data);

	ctx->m_f32.clear();
	ctx->m_animIndex++;
}

void RenderModel::__ParseDataSet_PositionTransformStack_Translate(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() < 3)
		return;

	Matrix4 matrix;
	matrix.SetIdentity();
	matrix.m[3][0] = ctx->m_f32[0];
	matrix.m[3][1] = ctx->m_f32[1];
	matrix.m[3][2] = ctx->m_f32[2];

	std::string& tagName = ctx->m_tagStack[ctx->m_tagStack.size() - 2];
	ctx->m_transformStack.push_back(std::make_pair(tagName, matrix));

	ctx->m_f32.clear();
}

void RenderModel::__ParseDataSet_PositionTransformStack_Matrix(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() < 16)
		return;

	Matrix4 matrix;
	matrix.m[0][0] = ctx->m_f32[0];
	matrix.m[1][0] = ctx->m_f32[1];
	matrix.m[2][0] = ctx->m_f32[2];
	matrix.m[3][0] = ctx->m_f32[3];
	matrix.m[0][1] = ctx->m_f32[4];
	matrix.m[1][1] = ctx->m_f32[5];
	matrix.m[2][1] = ctx->m_f32[6];
	matrix.m[3][1] = ctx->m_f32[7];
	matrix.m[0][2] = ctx->m_f32[8];
	matrix.m[1][2] = ctx->m_f32[9];
	matrix.m[2][2] = ctx->m_f32[10];
	matrix.m[3][2] = ctx->m_f32[11];
	matrix.m[0][3] = ctx->m_f32[12];
	matrix.m[1][3] = ctx->m_f32[13];
	matrix.m[2][3] = ctx->m_f32[14];
	matrix.m[3][3] = ctx->m_f32[15];

	std::string& tagName = ctx->m_tagStack[ctx->m_tagStack.size() - 2];
	ctx->m_transformStack.push_back(std::make_pair(tagName, matrix));

	ctx->m_f32.clear();
}

void RenderModel::__ParseDataSet_EffectEmission(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() == 4)
	{
		ctx->m_tempEffect.m_emission.x = ctx->m_f32[0];
		ctx->m_tempEffect.m_emission.y = ctx->m_f32[1];
		ctx->m_tempEffect.m_emission.z = ctx->m_f32[2];
		ctx->m_tempEffect.m_emission.w = ctx->m_f32[3];
		ctx->m_f32.clear();
	}
}

void RenderModel::__ParseDataSet_EffectDiffuse(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() == 4)
	{
		ctx->m_tempEffect.m_diffuse.x = ctx->m_f32[0];
		ctx->m_tempEffect.m_diffuse.y = ctx->m_f32[1];
		ctx->m_tempEffect.m_diffuse.z = ctx->m_f32[2];
		ctx->m_tempEffect.m_diffuse.w = ctx->m_f32[3];
		ctx->m_f32.clear();
	}
}

void RenderModel::__ParseDataSet_EffectSpecular(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_f32.size() == 4)
	{
		ctx->m_tempEffect.m_specular.x = ctx->m_f32[0];
		ctx->m_tempEffect.m_specular.y = ctx->m_f32[1];
		ctx->m_tempEffect.m_specular.z = ctx->m_f32[2];
		ctx->m_tempEffect.m_specular.w = ctx->m_f32[3];
		ctx->m_f32.clear();
	}
}

void RenderModel::__ParseDataSet_EffectTexture(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_buffer.empty())
		return;

	ctx->m_tempEffect.m_textureID = ctx->m_buffer;
	ctx->m_buffer.clear();
}

void RenderModel::__ParseDataSet_UpAxis(RenderModel_DAE_ParseContext* ctx)
{
	if (ctx->m_buffer.empty())
		return;

	ctx->m_coordTranslate.SetIdentity();
	if (ctx->m_buffer == "X_UP")
	{
		// we want Y to be up, so create a transform matrix to fix all vectors.
		ctx->m_modelUp = Vector3(1.f, 0.f, 0.f);
	}
	else if (ctx->m_buffer == "Y_UP")
	{
		// we want Y to be up, so create a transform matrix to fix all vectors.
		ctx->m_modelUp = Vector3(0.f, 1.f, 0.f);
	}
	else if (ctx->m_buffer == "Z_UP")
	{
		// we want Y to be up, so create a transform matrix to fix all vectors.
		ctx->m_modelUp = Vector3(0.f, 0.f, 1.f);

		ctx->m_coordTranslate.m[0][0] = -1.f;
		ctx->m_coordTranslate.m[1][1] = 0.f;
		ctx->m_coordTranslate.m[1][2] = -1.f;
		ctx->m_coordTranslate.m[2][1] = 1.f;
		ctx->m_coordTranslate.m[2][2] = 0.f;
	}
	else
	{
		assert(!"Unhandled up axis type");
	}
	ctx->m_buffer.clear();
}

void RenderModel::__ParseDAEEndElement(void *_ctx, const char* name)
{
	RenderModel_DAE_ParseContext* ctx = reinterpret_cast<RenderModel_DAE_ParseContext*>(_ctx);
	assert(_stricmp(ctx->m_tagStack.back().c_str(), name) == 0);
	HRESULT hr = S_OK;

	// finish up parsing CDATA.
	if (ctx->m_fnParseChars)
	{
		(*(ctx->m_fnParseBuffer))(ctx);
		ctx->m_fnParseChars = nullptr;
		ctx->m_fnParseBuffer = nullptr;
		ctx->m_fnParseDataSet = nullptr;
	}

	if (ctx->m_pMesh
		&& (ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 1].c_str(), "node") == 0)
		&& (ctx->m_idStack.back().second.find("POS_") != std::string::npos))
	{
		Matrix4 m;
		m.SetIdentity();

		for (std::vector<std::pair<std::string, Matrix4>>::iterator itTrans = ctx->m_transformStack.begin() + 1; itTrans != ctx->m_transformStack.end(); ++itTrans)
		{
			const Matrix4& b = (*itTrans).second;
			m = Matrix4::MultiplyAB(m, b);
		}

		RenderModel_NamedVertex node;
		node.m_name = ctx->m_idStack.back().second;
		node.m_name.erase(node.m_name.begin(), node.m_name.begin() + 4);
		node.m_vertex.x = m.m[3][0];
		node.m_vertex.y = m.m[3][1];
		node.m_vertex.z = m.m[3][2];
		ctx->m_pModel->m_namedVerticies.push_back(node);
	}

	// instance geometry.
	if ((ctx->m_tagStack.size() == 4)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0))
	{
		ctx->m_pMesh = nullptr;
	}

	// armature base joint matrix
	if ((ctx->m_tagStack.size() == 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& ((_stricmp(ctx->m_tagStack[4].c_str(), "translate") == 0)
			|| (_stricmp(ctx->m_tagStack[4].c_str(), "matrix") == 0))
		&& (ctx->m_idStack.size() == 2)
		&& (ctx->m_idStack[1].second.find("Armature") != std::string::npos)
		&& !ctx->m_transformStack.empty())
	{
		ctx->m_baseJointMatrix = ctx->m_transformStack.back().second;
	}

	// armature joint matrix
	if ((ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 2].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack[ctx->m_tagStack.size() - 1].c_str(), "matrix") == 0)
		&& (ctx->m_idStack[ctx->m_idStack.size() - 1].second.find("Armature") != std::string::npos)
		&& !ctx->m_skinJointStack.empty()
		&& !ctx->m_transformStack.empty())
	{
		RenderModel_DAE_SkinJoint* joint = ctx->m_skinJointStack.back();
		joint->m_jointMatrix = ctx->m_transformStack.back().second;
	}

	// armature joint node.
	if (!ctx->m_skinJointStack.empty()
		&& (ctx->m_tagStack.size() >= 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_visual_scenes") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "visual_scene") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "node") == 0)
		&& (_stricmp(ctx->m_tagStack.back().c_str(), "node") == 0))
	{
		ctx->m_skinJointStack.pop_back();
	}

	// skin.
	if ((ctx->m_tagStack.size() == 4)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_controllers") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "controller") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "skin") == 0))
	{
		ctx->m_pMesh = nullptr;
	}

	// triangles.
	if (ctx->m_pMesh
		&& (ctx->m_tagStack.size() == 5)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_geometries") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "geometry") == 0)
		&& (_stricmp(ctx->m_tagStack[3].c_str(), "mesh") == 0)
		&& (_stricmp(ctx->m_tagStack[4].c_str(), "triangles") == 0))
	{
		ctx->m_materialIndex = -1;
	}

	// geometry.
	if ((ctx->m_tagStack.size() == 3)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_geometries") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "geometry") == 0))
	{
		ctx->m_pMesh = nullptr;
	}

	// effect.
	if ((ctx->m_tagStack.size() == 3)
		&& (_stricmp(ctx->m_tagStack[0].c_str(), "COLLADA") == 0)
		&& (_stricmp(ctx->m_tagStack[1].c_str(), "library_effects") == 0)
		&& (_stricmp(ctx->m_tagStack[2].c_str(), "effect") == 0))
	{
		ctx->m_tempEffect.m_id = ctx->m_idStack.back().second;
		ctx->m_effects.push_back(ctx->m_tempEffect);
		ctx->m_tempEffect.Clear();
	}

	if (!ctx->m_transformStack.empty()
		&& (_stricmp(ctx->m_transformStack.back().first.c_str(), reinterpret_cast<const char*>(name)) == 0))
	{
		ctx->m_transformStack.pop_back();
	}

	if (!ctx->m_idStack.empty()
		&& (_stricmp(ctx->m_idStack.back().first.c_str(), reinterpret_cast<const char*>(name)) == 0))
	{
		ctx->m_idStack.pop_back();
	}

	ctx->m_tagStack.pop_back();
}

}

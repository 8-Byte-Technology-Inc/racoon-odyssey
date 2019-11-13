#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <map>

#include "common/basic_types.h"
#include "common/ref_count.h"
#include "common/file_io.h"
#include "common/parse_xml.h"

namespace TB8
{

class RenderMain;
class RenderTexture;
class RenderShader;
struct RenderModel_DAE_ParseContext;
struct RenderModel_DAE_Triangle;
struct RenderModel_DAE_SkinJoint;
struct RenderModel_DAE_Anim_Transform;
struct RenderModel_DAE_Mesh;
enum RenderMainViewType : u32;

struct RenderModel_VertexPositionTexture
{
	Vector3 pos;
	Vector2 tex;
};

struct RenderModel_NamedVertex
{
	std::string m_name;
	Vector3 m_vertex;
};

struct RenderModel_Bounds
{
	RenderModel_Bounds()
		: m_min(FLT_MAX, FLT_MAX, FLT_MAX)
		, m_max(-FLT_MAX, -FLT_MAX, -FLT_MAX)
	{
	}

	void AddVector(const Vector3& v);
	void ComputeCenterAndSize();

	Vector3							m_min;
	Vector3							m_max;
	Vector3							m_center;
	Vector3							m_size;
};

struct RenderModel_Mesh
{
	std::string						m_name;
	RenderModel_Bounds				m_bounds;
};

struct RenderModel_Joint
{
	RenderModel_Joint()
		: m_index(-1)
		, m_parentIndex(-1)
		, m_isDirty(false)
	{
	}

	std::string						m_name;

	s32								m_index;
	s32								m_parentIndex;

	Matrix4							m_baseMatrix;
	Matrix4							m_baseInvBindMatrix;

	bool							m_isDirty;
	Matrix4							m_effectiveMatrix;
	Matrix4							m_computedMatrix;
	Matrix4							m_boundMatrix;
};

struct RenderModel_Anim_Joint
{
	RenderModel_Joint*						m_pJoint;
	Matrix4									m_transform;
};

struct RenderModel_Anim
{
	RenderModel_Anim()
		: m_animIndex(-1)
		, m_animID(-1)
		, m_animTime(0.f)
	{
	}

	s32										m_animIndex;
	s32										m_animID;
	f32										m_animTime;

	RenderModel_Bounds						m_bounds;

	std::vector<RenderModel_Anim_Joint>		m_joints;
};

class RenderModel : public ref_count
{
public:
	RenderModel(RenderMain* pRenderer);
	~RenderModel();

	static RenderModel* Alloc(RenderMain* pRenderer, s32 vertexCount, RenderModel_VertexPositionTexture* verticies, s32 indexCount, u16* indicies, const Vector4& color);
	static RenderModel* AllocFromDAE(RenderMain* pRenderer, const char* path, const char* file, const char* modelName);
	static RenderModel* AllocSimpleRectangle(RenderMain* pRenderer, RenderMainViewType viewType, const Vector3& v0, const Vector3& v1, RenderTexture* pTexture, const Vector2& uv0, const Vector2& uv1);

	void SetPosition(const Vector3& position);
	void SetRotation(const Vector3& rotation);
	void SetScale(const Vector3& scale);
	void SetWorldTransform(const Matrix4& transform);
	const Matrix4& GetWorldTransform() const { return m_worldTransform; }
	const Vector3& GetCenter() const { return m_meshes.front().m_bounds.m_center; }
	const Vector3& GetSize() const { return m_meshes.front().m_bounds.m_size; }

	const std::vector<RenderModel_Anim>& GetAnims() const { return m_anims; }
	u32 GetAnimCount() const;
	const RenderModel_Anim* GetAnim(u32 animID) const;
	const std::vector<RenderModel_Mesh>& GetMeshes() const { return m_meshes; }
	void SetAnimID(const s32 animID);

	u32 GetJointCount() const { return m_cJoints; }
	void SetJointRotation(u32 jointIndex, const Vector3& rotation);
	void ResetJointTransformMatricies();
	void SetJointTransformMatrix(u32 jointIndex, const Matrix4& rotation);
	const RenderModel_Joint* GetJoint(s32 jointIndex) { return &(m_joints[jointIndex]); }
	const RenderModel_Anim_Joint* GetAnimJoint(s32 animID, s32 jointIndex) { return __GetAnimJoint(animID, jointIndex); }

	void GetNamedVerticies(std::vector<RenderModel_NamedVertex>* pVerticies) const;

	void Render(RenderMain* pRenderer);

private:
	void __Initialize(RenderMain* pRenderer, s32 vertexCount, RenderModel_VertexPositionTexture* verticies, s32 indexCount, u16* indicies, const Vector4& color);
	void __InitializeFromDAE(RenderMain* pRenderer, const char* path, const char* file, const char* modelName);
	void __InitializeSimpleRectangle(RenderMain* pRenderer, RenderMainViewType viewType, const Vector3& v0, const Vector3& v1, RenderTexture* pTexture, const Vector2& uv0, const Vector2& uv1);

	virtual void __Free() override;

	void __InitVSConstantBuffers();
	void __UpdateVSConstants_World();
	void __UpdateVSConstants_Anim();
	void __UpdateVSConstants_Joints();
	void __UpdateJointMatricies();
	RenderModel_NamedVertex* __FindNamedVertex(const char* name);
	const RenderModel_Anim_Joint* __GetAnimJoint(s32 animID, s32 jointIndex);

	// DAE helpers.
	void __CheckVertexOrder(RenderModel_DAE_ParseContext& parseContext, RenderModel_DAE_Mesh& mesh, RenderModel_DAE_Triangle& triangle);
	Matrix4 __ComputeJointModelTransform(RenderModel_DAE_ParseContext& parseContext, RenderModel_DAE_SkinJoint& joint, s32 animID);
	RenderModel_DAE_Anim_Transform* __LookupAnimTransform(RenderModel_DAE_ParseContext& parseContext, const char* jointName, s32 animID);
	void __SetBoneTextureData(RenderModel_DAE_ParseContext& parseContext, f32* pBoneTextureData, const IVector2& boneTextureSize, const RenderModel_Anim& anim);

	static void __ParseDAEStartElement(void *ctx, const u8* name, const u8** atts);
	static void __ParseDAECharacters(void *ctx, const u8* value, int len);
	static void __ParseDAEEndElement(void *ctx, const u8* name);
	static XML_Parser_Result __ParseDAERead(TB8::File* f, u8* pBuf, u32* pSize);

	static bool __ParseChars_Numbers(RenderModel_DAE_ParseContext* ctx, char v);
	static bool __ParseChars_SingleString(RenderModel_DAE_ParseContext* ctx, char v);
	static bool __ParseChars_MultiString(RenderModel_DAE_ParseContext* ctx, char v);

	static void __ParseBuffer_F32(RenderModel_DAE_ParseContext* ctx);
	static void __ParseBuffer_S32(RenderModel_DAE_ParseContext* ctx);
	static void __ParseBuffer_SingleString(RenderModel_DAE_ParseContext* ctx);
	static void __ParseBuffer_MultiString(RenderModel_DAE_ParseContext* ctx);

	static void __ParseDataSet_MeshVertex(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_MeshNormal(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_MeshMap(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_MeshTriangle(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_TextureFileName(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_JointName(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_BindShapeMatrix(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_JointInvBindMatrix(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_SkinWeight(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_VertexWeightCounts(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_VertexWeights(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_JointMatrix(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_AnimIDs(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_AnimMatrix(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_PositionTransformStack_Matrix(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_PositionTransformStack_Translate(RenderModel_DAE_ParseContext* ctx);

	static void __ParseDataSet_EffectEmission(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_EffectDiffuse(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_EffectSpecular(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_EffectTexture(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_UpAxis(RenderModel_DAE_ParseContext* ctx);
	
	RenderMain*								m_pRenderer;

	s32										m_vertexCount;
	ID3D11Buffer*							m_pVertexBuffer;

	s32										m_indexCount;
	ID3D11Buffer*							m_pIndexBuffer;

	RenderTexture*							m_pTexture;

	RenderShader*							m_pShader;

	Vector3									m_position;
	Vector3									m_rotation;
	Vector3									m_scale;

	Matrix4									m_coordTranslate;
	Matrix4									m_worldTransform;

	Vector3									m_center;

	std::vector<RenderModel_Anim>			m_anims;
	s32										m_animIndex;
	ID3D11ShaderResourceView*				m_pBoneTexture;

	u32										m_cJoints;
	bool									m_isJointsDirty;
	Matrix4									m_baseJointMatrix;
	Matrix4									m_bindShapeMatrix;
	RenderModel_Joint						m_joints[16];

	std::vector<RenderModel_Mesh>			m_meshes;

	RenderMainViewType						m_viewType;

	ID3D11Buffer*							m_pVSConstantBuffer_World;
	ID3D11Buffer*							m_pVSConstantBuffer_Anim;
	ID3D11Buffer*							m_pVSConstantBuffer_Joints;

	std::vector<RenderModel_NamedVertex>	m_namedVerticies;
};

}

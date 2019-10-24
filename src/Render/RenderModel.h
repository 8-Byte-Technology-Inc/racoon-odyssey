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

struct RenderModel_Mesh
{
	std::string						m_name;
	Vector3							m_min;
	Vector3							m_max;
	Vector3							m_size;
};

struct RenderModel_Anims
{
	s32								m_animIndex;
	s32								m_animID;
	f32								m_animTime;
};

class RenderModel : public ref_count
{
public:
	RenderModel(RenderMain* pRenderer);
	~RenderModel();

	static RenderModel* Alloc(RenderMain* pRenderer, s32 vertexCount, RenderModel_VertexPositionTexture* verticies, s32 indexCount, u16* indicies, const Vector4& color);
	static RenderModel* AllocFromDAE(RenderMain* pRenderer, const char* path, const char* file, const char* modelName);

	void SetPosition(const Vector3& position);
	void SetRotation(const Vector3& rotation);
	void SetScale(const Vector3& scale);
	const Matrix4& GetWorldTransform() const { return m_worldTransform; }

	const std::vector<RenderModel_Anims>& GetAnims() const { return m_anims; }
	const std::vector<RenderModel_Mesh>& GetMeshes() const { return m_meshes; }
	void SetAnimID(const s32 animID);

	void GetNamedVerticies(std::vector<RenderModel_NamedVertex>* pVerticies) const;

	void Render(RenderMain* pRenderer);

private:
	void __Initialize(RenderMain* pRenderer, s32 vertexCount, RenderModel_VertexPositionTexture* verticies, s32 indexCount, u16* indicies, const Vector4& color);
	void __InitializeFromDAE(RenderMain* pRenderer, const char* path, const char* file, const char* modelName);

	virtual void __Free() override;

	void __InitVSConstantBuffer();
	void __UpdateVSConstants();
	RenderModel_NamedVertex* __FindNamedVertex(const char* name);

	// DAE helpers.
	void __CheckVertexOrder(RenderModel_DAE_ParseContext& parseContext, RenderModel_DAE_Mesh& mesh, RenderModel_DAE_Triangle& triangle);
	Matrix4 __ComputeJointModelTransform(RenderModel_DAE_ParseContext& parseContext, RenderModel_DAE_SkinJoint& joint, s32 animID);
	RenderModel_DAE_Anim_Transform* __LookupAnimTransform(RenderModel_DAE_ParseContext& parseContext, const char* jointName, s32 animID);
	void __SetBoneTextureData(RenderModel_DAE_ParseContext& parseContext, f32* pBoneTextureData, const IVector2& boneTextureSize, const RenderModel_Anims& anim);
	void __UpdateLimits(RenderModel_DAE_Mesh& mesh, const Vector3& src);

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
	static void __ParseDataSet_JointInvBindMatrix(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_SkinWeight(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_VertexWeightCounts(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_VertexWeights(RenderModel_DAE_ParseContext* ctx);
	static void __ParseDataSet_BaseJointVector(RenderModel_DAE_ParseContext* ctx);
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
	
	RenderMain*							m_pRenderer;

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

	std::vector<RenderModel_Anims>			m_anims;
	s32										m_animIndex;
	ID3D11ShaderResourceView*				m_pBoneTexture;

	std::vector<RenderModel_Mesh>			m_meshes;

	ID3D11Buffer*							m_pVSConstantBuffer;

	std::vector<RenderModel_NamedVertex>	m_namedVerticies;
};

}

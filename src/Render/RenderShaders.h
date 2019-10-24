#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

#include "common/basic_types.h"
#include "common/ref_count.h"

namespace TB8
{

class RenderTexture;
class RenderMain;

enum RenderShaderID : u32
{
	RenderShaderID_Invalid = 0,
	RenderShaderID_Generic,
};

struct RenderShader_Vertex_Generic
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT4 color;
	DirectX::XMINT4 bones;
	DirectX::XMFLOAT4 weights;
};

struct RenderShaders_Model_VSConstantants
{
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 worldNormalMatrix;
	int32_t animIndex;
};

class RenderShader : public TB8::ref_count
{
public:
	RenderShader(RenderMain* pRenderer, RenderShaderID id);
	~RenderShader();

	static RenderShader* Alloc(RenderMain* renderer, RenderShaderID id, const char* path, const char* vertexFileName, const char* pixelFileName);

	RenderShaderID GetID() const { return m_id; }

	void SetViewTransform(const Matrix4& view);
	void SetProjectionTransform(const Matrix4& view);
	void SetLightVector(const Vector3& vector);

	void SetTexture(RenderTexture* pTextures);
	void SetBoneTexture(ID3D11ShaderResourceView* pBoneTexture);
	void SetModelVSConstants(ID3D11Buffer* pVSConstants);

	void ApplyRenderState(ID3D11DeviceContext* context);

private:
	void __Initialize(const char* path, const char* vertexFileName, const char* pixelFileName);
	void __UpdateVSConstants();
	void __UpdatePSConstants();
	void __Shutdown();
	virtual void __Free() override;

	RenderMain*								m_pRenderer;
	RenderShaderID							m_id;

	DirectX::XMMATRIX						m_viewTransform;
	DirectX::XMMATRIX						m_projectionTransform;

	DirectX::XMVECTOR						m_lightVector;

	ID3D11VertexShader*						m_pVertexShader;
	ID3D11InputLayout*						m_pInputLayout;
	ID3D11PixelShader*						m_pPixelShader;
	ID3D11Buffer*							m_pVSConstantBuffer;
	ID3D11Buffer*							m_pVSConstantBufferModel;
	ID3D11Buffer*							m_pPSConstantBuffer;
	ID3D11SamplerState*						m_pSampleState;
	RenderTexture*							m_pTexture;
	ID3D11ShaderResourceView*				m_pBoneTexture;
};

}

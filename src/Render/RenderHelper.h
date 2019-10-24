#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include <vector>

#include "common/basic_types.h"

namespace TB8
{

void Matrix4ToXMMATRIX(const Matrix4& src, DirectX::XMMATRIX& dst);
void XMMATRIXToMatrix4(const DirectX::XMMATRIX& src, Matrix4& dst);

void XMVECTORToVector3(const DirectX::XMVECTOR& src, Vector3& dst);

void Vector4ToXMVECTOR(const Vector4& src, DirectX::XMVECTOR& dst);
void Vector3ToXMVECTOR(const Vector3& src, DirectX::XMVECTOR& dst);

void IRectToD2D1_RECT_F(const IRect& src, D2D1_RECT_F& dst);

void IVector2ToD2D1_POINT_F(const IVector2& src, D2D1_POINT_2F* dst);

HRESULT LoadFile(const char* filePath, std::vector<u8>& dst);

}

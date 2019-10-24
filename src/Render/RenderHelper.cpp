#include "pch.h"

#include "common/file_io.h"

#include "RenderHelper.h"

namespace TB8
{

void Matrix4ToXMMATRIX(const Matrix4& src, DirectX::XMMATRIX& dst)
{
	dst.r[0].m128_f32[0] = src.m[0][0];
	dst.r[0].m128_f32[1] = src.m[0][1];
	dst.r[0].m128_f32[2] = src.m[0][2];
	dst.r[0].m128_f32[3] = src.m[0][3];

	dst.r[1].m128_f32[0] = src.m[1][0];
	dst.r[1].m128_f32[1] = src.m[1][1];
	dst.r[1].m128_f32[2] = src.m[1][2];
	dst.r[1].m128_f32[3] = src.m[1][3];

	dst.r[2].m128_f32[0] = src.m[2][0];
	dst.r[2].m128_f32[1] = src.m[2][1];
	dst.r[2].m128_f32[2] = src.m[2][2];
	dst.r[2].m128_f32[3] = src.m[2][3];

	dst.r[3].m128_f32[0] = src.m[3][0];
	dst.r[3].m128_f32[1] = src.m[3][1];
	dst.r[3].m128_f32[2] = src.m[3][2];
	dst.r[3].m128_f32[3] = src.m[3][3];
}

void XMMATRIXToMatrix4(const DirectX::XMMATRIX& src, Matrix4& dst)
{
	dst.m[0][0] = src.r[0].m128_f32[0];
	dst.m[0][1] = src.r[0].m128_f32[1];
	dst.m[0][2] = src.r[0].m128_f32[2];
	dst.m[0][3] = src.r[0].m128_f32[3];

	dst.m[1][0] = src.r[1].m128_f32[0];
	dst.m[1][1] = src.r[1].m128_f32[1];
	dst.m[1][2] = src.r[1].m128_f32[2];
	dst.m[1][3] = src.r[1].m128_f32[3];

	dst.m[2][0] = src.r[2].m128_f32[0];
	dst.m[2][1] = src.r[2].m128_f32[1];
	dst.m[2][2] = src.r[2].m128_f32[2];
	dst.m[2][3] = src.r[2].m128_f32[3];

	dst.m[3][0] = src.r[3].m128_f32[0];
	dst.m[3][1] = src.r[3].m128_f32[1];
	dst.m[3][2] = src.r[3].m128_f32[2];
	dst.m[3][3] = src.r[3].m128_f32[3];
}

void XMVECTORToVector3(const DirectX::XMVECTOR& src, Vector3& dst)
{
	dst.x = src.m128_f32[0];
	dst.y = src.m128_f32[1];
	dst.z = src.m128_f32[2];
}

void Vector4ToXMVECTOR(const Vector4& src, DirectX::XMVECTOR& dst)
{
	dst.m128_f32[0] = src.x;
	dst.m128_f32[1] = src.y;
	dst.m128_f32[2] = src.z;
	dst.m128_f32[3] = src.u;
}

void Vector3ToXMVECTOR(const Vector3& src, DirectX::XMVECTOR& dst)
{
	dst.m128_f32[0] = src.x;
	dst.m128_f32[1] = src.y;
	dst.m128_f32[2] = src.z;
	dst.m128_f32[3] = 0.f;
}

void IRectToD2D1_RECT_F(const IRect& src, D2D1_RECT_F& dst)
{
	dst.left = static_cast<float>(src.left);
	dst.right = static_cast<float>(src.right);
	dst.top = static_cast<float>(src.top);
	dst.bottom = static_cast<float>(src.bottom);
}

void IVector2ToD2D1_POINT_F(const IVector2& src, D2D1_POINT_2F* dst)
{
	dst->x = static_cast<float>(src.x);
	dst->y = static_cast<float>(src.y);
}

HRESULT LoadFile(const char* filePath, std::vector<u8>& dst)
{
	TB8::File* f = TB8::File::AllocOpen(filePath, true);
	if (!f)
		return HRESULT_FROM_WIN32(GetLastError());

	f->Read(&dst);
	OBJFREE(f);
	return S_OK;
}

}

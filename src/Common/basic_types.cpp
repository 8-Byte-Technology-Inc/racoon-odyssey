/*
	Copyright (C) 2019 8 Byte Technology Inc. - All Rights Reserved
*/
#include "pch.h"

#include <cctype>
#include <string>
#include <math.h>

#include "basic_types.h"

std::size_t u128_hasher::operator()(const u128& obj) const
{
	return obj.low ^ obj.high;
}

void Vector2::Normalize()
{
	f32 len = static_cast<f32>(sqrt((x * x) + (y * y)));
	x = x / len;
	y = y / len;
}

Vector3 Vector3::Cross(const Vector3& u, const Vector3& v)
{
	return Vector3((u.y * v.z - u.z * v.y), (u.z * v.x - u.x * v.z), (u.x * v.y - u.y * v.x));
}

Vector3 Vector3::ComputeNormal(const Vector3& o, const Vector3& p1, const Vector3& p2)
{
	const Vector3 u(p1.x - o.x, p1.y - o.y, p1.z - o.z);
	const Vector3 v(p2.x - o.x, p2.y - o.y, p2.z - o.z);
	const Vector3 x = Vector3::Cross(u, v);
	return Vector3::Normalize(x);
}

Vector3 operator +(const Vector3& a, const Vector3& b)
{
	return Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vector3 operator -(const Vector3& a, const Vector3& b)
{
	return Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vector3 operator *(const Vector3& a, f32 b)
{
	return Vector3(a.x * b, a.y * b, a.z * b);
}

Vector3 operator /(const Vector3& a, f32 b)
{
	return Vector3(a.x / b, a.y / b, a.z / b);
}

Vector2 operator +(const Vector2& a, const Vector2& b)
{
	return Vector2(a.x + b.x, a.y + b.y);
}

Vector2 operator -(const Vector2& a, const Vector2& b)
{
	return Vector2(a.x - b.x, a.y - b.y);
}

Vector2 operator *(const Vector2& a, f32 b)
{
	return Vector2(a.x * b, a.y * b);
}

Vector2 operator /(const Vector2& a, f32 b)
{
	return Vector2(a.x / b, a.y / b);
}

Vector4 Vector4::SLERP(const Vector4& qa, const Vector4& qb, f32 t)
{
	Vector4 qr;
	f32 cosAB = Vector4::Dot(qa, qb);
	if (std::abs(cosAB) >= 1.f)
		return qa;
	const f32 sign = get_sign(cosAB);
	const f32 angleAB = static_cast<f32>(acos(sign * cosAB));
	const f32 sinAB = static_cast<f32>(sin(angleAB));
	if (is_approx_zero(std::abs(sinAB)))
	{
		qr.x = (1.f - t) * qa.x + sign * t * qb.x;
		qr.y = (1.f - t) * qa.y + sign * t * qb.y;
		qr.z = (1.f - t) * qa.z + sign * t * qb.z;
		qr.w = (1.f - t) * qa.w + sign * t * qb.w;
	}
	else
	{
		const f32 f_a = static_cast<f32>(sin((1.f - t) * angleAB)) / sinAB;
		const f32 f_b = static_cast<f32>(sin(t * angleAB)) / sinAB;

		qr.x = f_a * qa.x + sign * f_b * qb.x;
		qr.y = f_a * qa.y + sign * f_b * qb.y;
		qr.z = f_a * qa.z + sign * f_b * qb.z;
		qr.w = f_a * qa.w + sign * f_b * qb.w;
	}
	return qr;
}

IVector2 IVector2::Rotate(s32 deg) const
{
	if (deg == 0)
		return *this;
	else if (deg == 90)
		return IVector2(-y, x);
	else if (deg == 180)
		return IVector2(-x, -y);
	else if (deg == 270)
		return IVector2(y, -x);
	else
	{
		assert(false);
		return *this;
	}
}

bool IVector2::operator ==(const IVector2& rhs) const
{
	return x == rhs.x && y == rhs.y;
}

bool IVector2::operator !=(const IVector2& rhs) const
{
	return x != rhs.x || y != rhs.y;
}

IVector2& IVector2::operator +=(const IVector2& rhs)
{
	x += rhs.x;
	y += rhs.y;
	return *this;
}

IVector2 IVector2::FromString(const char* value)
{
	IVector2 data;

	const char* i1 = value;
	const char* i2 = i1;
	s32 cParsed = 0;
	while (true)
	{
		if (std::isdigit(*i2) || *i2 == '-' || *i2 == '+')
		{
			++i2;
		}
		else
		{
			if (i1 < i2)
			{
				if (cParsed == 0)
				{
					data.x = static_cast<s32>(std::stoul(i1, nullptr, 10));
				}
				else if (cParsed == 1)
				{
					data.y = static_cast<s32>(std::stoul(i1, nullptr, 10));
				}
			}
			if (!*i2)
				break;
			i1 = i2 + 1;
			i2 = i1;
			++cParsed;
		}
	}

	return data;
}

bool IVector2::operator <(const IVector2& rhs) const
{
	s32 diff = y - rhs.y;
	if (diff != 0)
		return diff < 0;

	diff = x - rhs.x;
	if (diff != 0)
		return diff < 0;

	return false;
}

bool IVector2::operator >(const IVector2& rhs) const
{
	s32 diff = y - rhs.y;
	if (diff != 0)
		return diff > 0;

	diff = x - rhs.x;
	if (diff != 0)
		return diff > 0;

	return false;
}

s32 IVector2::DistSq(const IVector2& lhs, const IVector2& rhs)
{
	const s32 diffX = lhs.x - rhs.x;
	const s32 diffY = lhs.y - rhs.y;
	return diffX * diffX + diffY * diffY;
}

IVector2 IVector2::GetFacing(const IVector2& src, const IVector2& dst)
{
	return IVector2((src.x == dst.x) ? 0 : (src.x > dst.x ? -1 : +1), (src.y == dst.y) ? 0 : (src.y > dst.y ? -1 : +1));
}

IVector2 IVector2::GetFacing(const IVector2& src, const IRect& dst)
{
	IVector2 facing;
	if (src.x < dst.left)
		facing.x = +1;
	else if (src.x >= dst.right)
		facing.x = -1;
	if (src.y < dst.top)
		facing.y = +1;
	else if (src.y >= dst.bottom)
		facing.y = -1;
	return facing;
}

s32 IVector2::GetRotationFromFacing(const IVector2& facing)
{
	if (facing.x == 0 && facing.y < 0)
	{
		return 0;
	}
	else if (facing.x > 0 && facing.y < 0)
	{
		return 45;
	}
	else if (facing.x > 0 && facing.y == 0)
	{
		return 90;
	}
	else if (facing.x > 0 && facing.y > 0)
	{
		return 135;
	}
	else if (facing.x == 0 && facing.y > 0)
	{
		return 180;
	}
	else if (facing.x < 0 && facing.y > 0)
	{
		return 225;
	}
	else if (facing.x < 0 && facing.y == 0)
	{
		return 270;
	}
	else if (facing.x < 0 && facing.y < 0)
	{
		return 315;
	}
	else
	{
		return 0;
	}
}

IVector2 IVector2::RotatePos(const IVector2& pos, s32 rotation)
{
	if (rotation == 0)
		return pos;
	else if (rotation == 90)
		return IVector2(-pos.y, pos.x);
	else if (rotation == 180)
		return IVector2(-pos.x, -pos.y);
	else if (rotation == 270)
		return IVector2(pos.y, -pos.x);
	else
	{
		assert(!"unhandled rotation");
		return IVector2();
	}
}

IVector2 IVector2::RotatePosInRect(const IVector2& _pos, const IVector2& _size, s32 rotation)
{
	switch (rotation)
	{
	case 0:
		return _pos;
	case 90:
		return IVector2(_size.y - 1 - _pos.y, _pos.x);
	case 180:
		return IVector2(_size.x - 1 - _pos.x, _size.y - 1 - _pos.y);
	case 270:
		return IVector2(_pos.y, _size.x - 1 - _pos.x);
	default:
		assert(!"unhandled rotation");
		return IVector2();
	}
}

IRect IRect::FromString(const char* value)
{
	IRect data;

	const char* i1 = value;
	const char* i2 = i1;
	s32 cParsed = 0;
	while (true)
	{
		if (std::isdigit(*i2))
		{
			++i2;
		}
		else
		{
			if (i1 < i2)
			{
				if (cParsed == 0)
				{
					data.left = static_cast<s32>(std::stoul(i1, nullptr, 10));
				}
				else if (cParsed == 1)
				{
					data.top = static_cast<s32>(std::stoul(i1, nullptr, 10));
				}
				else if (cParsed == 2)
				{
					data.right = static_cast<s32>(std::stoul(i1, nullptr, 10));
				}
				else if (cParsed == 3)
				{
					data.bottom = static_cast<s32>(std::stoul(i1, nullptr, 10));
				}
			}
			if (!*i2)
				break;
			i1 = i2 + 1;
			i2 = i1;
			++cParsed;
		}
	}

	return data;
}

bool IRect::Intersect(const IRect& r) const
{
	if (r.right <= left)
	{
		return false;
	}
	if (right <= r.left)
	{
		return false;
	}
	if (r.bottom <= top)
	{
		return false;
	}
	if (bottom <= r.top)
	{
		return false;
	}
	return true;
}

bool IRect::Intersect(const IVector2& pos) const
{
	return (left <= pos.x)
		&& (top <= pos.y)
		&& (pos.x < right)
		&& (pos.y < bottom);
}

bool IRect::Contain(const IRect &r) const
{
	return (left <= r.left)
		&& (top <= r.top)
		&& (right >= r.right)
		&& (bottom >= r.bottom);
}


void IRect::Inflate(s32 amount)
{
	left -= amount;
	top -= amount;
	right += amount;
	bottom += amount;
}

bool IRect::operator <(const IRect& rhs) const
{
	s32 diff = top - rhs.top;
	if (diff != 0)
		return diff < 0;

	diff = left - rhs.left;
	if (diff != 0)
		return diff < 0;

	diff = bottom - rhs.bottom;
	if (diff != 0)
		return diff < 0;

	diff = right - rhs.right;
	if (diff != 0)
		return diff < 0;

	return false;
}

bool IRect::operator ==(const IRect& rhs) const
{
	return (left == rhs.left) && (top == rhs.top) && (right == rhs.right) && (bottom == rhs.bottom);
}

bool IRect::operator !=(const IRect& rhs) const
{
	return (left != rhs.left) || (top != rhs.top) || (right != rhs.right) || (bottom != rhs.bottom);
}

s32 IRect::NearestDistSq(const IRect& rect, const IVector2& point)
{
	if (rect.right <= point.x && rect.bottom <= point.y)
	{
		return IVector2::DistSq(point, IVector2(rect.right, rect.bottom));
	}
	else if (rect.left > point.x && rect.bottom <= point.y)
	{
		return IVector2::DistSq(point, IVector2(rect.left, rect.bottom));
	}
	else if (rect.left > point.x && rect.top > point.y)
	{
		return IVector2::DistSq(point, IVector2(rect.left, rect.top));
	}
	else if (rect.left > point.x && rect.top > point.y)
	{
		return IVector2::DistSq(point, IVector2(rect.left, rect.top));
	}
	else if (rect.bottom <= point.y)
	{
		return (point.y - rect.bottom) * (point.y - rect.bottom);
	}
	else if (rect.top > point.y)
	{
		return (rect.top - point.y) * (rect.top - point.y);
	}
	else if (rect.left <= point.x)
	{
		return (point.x - rect.left) * (point.x - rect.left);
	}
	else if (rect.right > point.x)
	{
		return (rect.right - point.x) * (rect.right - point.x);
	}
	else
	{
		return 0;
	}
}

IRect IRect::RotateRectInRect(const IRect& _pos, const IVector2& size, s32 rotation)
{
	switch (rotation)
	{
	case 0:
		return _pos;
	case 90:
		return IRect(size.y - 1 - _pos.bottom, _pos.left, size.x - 1 - _pos.top, _pos.right);
	case 180:
		return IRect(size.x - 1 - _pos.right, size.y - 1 - _pos.bottom, size.x - 1 - _pos.left, size.y - 1 - _pos.top);
	case 270:
		return IRect(_pos.top, size.x - 1 - _pos.right, _pos.bottom, size.x - 1 - _pos.left);
	default:
		assert(!"unhandled rotation");
		return IRect();
	}
}

#if 0
IVector2 IVector2::RotatePosInRect(const IVector2& _pos, const IVector2& _size, s32 rotation)
{
	switch (rotation)
	{
	case 0:
		return _pos;
	case 90:
		return IVector2(_size.y - 1 - _pos.y, _pos.x);
	case 180:
		return IVector2(_size.x - 1 - _pos.x, _size.y - 1 - _pos.y);
	case 270:
		return IVector2(_pos.y, _size.x - 1 - _pos.x);
	default:
		assert(!"unhandled rotation");
		return IVector2();
	}
}
#endif


Matrix4::Matrix4()
{
	Clear();
}

void Matrix4::Clear()
{
	ZeroMemory(m, sizeof(m));
}

bool Matrix4::operator ==(const Matrix4& rhs) const
{
	return m[0][0] == rhs.m[0][0]
		&& m[0][1] == rhs.m[0][1]
		&& m[0][2] == rhs.m[0][2]
		&& m[0][3] == rhs.m[0][3]
		&& m[1][0] == rhs.m[1][0]
		&& m[1][1] == rhs.m[1][1]
		&& m[1][2] == rhs.m[1][2]
		&& m[1][3] == rhs.m[1][3]
		&& m[2][0] == rhs.m[2][0]
		&& m[2][1] == rhs.m[2][1]
		&& m[2][2] == rhs.m[2][2]
		&& m[2][3] == rhs.m[2][3]
		&& m[3][0] == rhs.m[3][0]
		&& m[3][1] == rhs.m[3][1]
		&& m[3][2] == rhs.m[3][2]
		&& m[3][3] == rhs.m[3][3];
}

void Matrix4::SetIdentity()
{
	Clear();
	m[0][0] = 1.0;
	m[1][1] = 1.0;
	m[2][2] = 1.0;
	m[3][3] = 1.0;
}

void Matrix4::SetScale(const Vector3& v)
{
	Clear();
	m[0][0] = v.x;
	m[1][1] = v.y;
	m[2][2] = v.z;
	m[3][3] = 1.0;
}

void Matrix4::SetTranslation(const Vector3& v)
{
	SetIdentity();
	AddTranslation(v);
}

void Matrix4::AddTranslation(const Vector3& v)
{
	m[3][0] = v.x;
	m[3][1] = v.y;
	m[3][2] = v.z;
}

void Matrix4::GetTranslation(Vector3& v) const
{
	v.x = m[3][0];
	v.y = m[3][1];
	v.z = m[3][2];
}

void Matrix4::SetRotate(const Vector3& v)
{
	Matrix4 rotateX;
	rotateX.SetRotateX(v.x);

	Matrix4 rotateY;
	rotateY.SetRotateY(v.y);

	Matrix4 rotateZ;
	rotateZ.SetRotateZ(v.z);

	*this = rotateX * rotateY * rotateZ;
}

void Matrix4::SetRotateX(const f32 rad)
{
	Clear();

	const f32 s = static_cast<f32>(sin(rad));
	const f32 c = static_cast<f32>(cos(rad));

	m[0][0] = 1.0f;
	m[1][1] = c;
	m[1][2] = s;
	m[2][1] = 0.0f - s;
	m[2][2] = c;
	m[3][3] = 1.0f;
}

void Matrix4::SetRotateY(const f32 rad)
{
	Clear();

	const f32 s = static_cast<f32>(sin(rad));
	const f32 c = static_cast<f32>(cos(rad));

	m[0][0] = c;
	m[0][2] = 0.0f - s;
	m[1][1] = 1.0f;
	m[2][0] = s;
	m[2][2] = c;
	m[3][3] = 1.0f;
}

void Matrix4::SetRotateZ(const f32 rad)
{
	Clear();

	const f32 s = static_cast<f32>(sin(rad));
	const f32 c = static_cast<f32>(cos(rad));

	m[0][0] = c;
	m[0][1] = s;
	m[1][0] = 0.0f - s;
	m[1][1] = c;
	m[2][2] = 1.0f;
	m[3][3] = 1.0f;
}

f32 Matrix4::Determinant() const
{
	return +m[0][0] * (+m[1][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2])
			 		   -m[1][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1])
			 		   +m[1][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]))
		   -m[0][1] * (+m[1][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2])
					   -m[1][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0])
					   +m[1][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]))
		   +m[0][2] * (+m[1][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1])
					   -m[1][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0])
					   +m[1][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]))
		   -m[0][3] * (+m[1][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1])
					   -m[1][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0])
					   +m[1][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
}

Vector4 Matrix4::ToQuaternion(const Matrix4& m)
{
	Vector4 q;

	const f32 tr0 = m.m[0][0];
	const f32 tr1 = m.m[1][1];
	const f32 tr2 = m.m[2][2];
	const f32 tr = tr0 + tr1 + tr2;

	if (tr > 0.f)
	{
		const f32 k = 0.5f / static_cast<f32>(sqrt(1.f + tr));
		q.x = k * (m.m[1][2] - m.m[2][1]);
		q.y = k * (m.m[2][0] - m.m[0][2]);
		q.z = k * (m.m[0][1] - m.m[1][0]);
		q.w = 0.25f / k;
	}
	else if ((tr0 > tr1) && (tr0 > tr2))
	{
		const f32 k = 0.5f / static_cast<f32>(sqrt(1.f + tr0 - tr1 - tr2));
		q.x = 0.25f / k;
		q.y = k * (m.m[1][0] + m.m[0][1]);
		q.z = k * (m.m[2][0] + m.m[0][2]);
		q.w = k * (m.m[1][2] - m.m[2][1]);
	}
	else if (tr1 > tr2)
	{
		const f32 k = 0.5f / static_cast<f32>(sqrt(1.f + tr1 - tr0 - tr2));
		q.x = k * (m.m[1][0] + m.m[0][1]);
		q.y = 0.25f / k;
		q.z = k * (m.m[2][1] + m.m[1][2]);
		q.w = k * (m.m[2][0] - m.m[0][2]);
	}
	else
	{
		const f32 k = 0.5f / static_cast<f32>(sqrt(1.f + tr2 - tr0 - tr1));
		q.x = k * (m.m[2][0] + m.m[0][2]);
		q.y = k * (m.m[2][1] + m.m[1][2]);
		q.z = 0.25f / k;
		q.w = k * (m.m[0][1] - m.m[1][0]);
	}

	return q;
}

Matrix4 Matrix4::FromQuaternion(const Vector4& q)
{
	Matrix4 m;

	m.m[0][0] = 1.f - 2.f * q.y * q.y - 2.f * q.z * q.z;
	m.m[0][1] = 2.f * q.x * q.y + 2.f * q.z * q.w;
	m.m[0][2] = 2.f * q.x * q.z - 2.f * q.y * q.w;

	m.m[1][0] = 2.f * q.x * q.y - 2.f * q.z * q.w;
	m.m[1][1] = 1.f - 2.f * q.x * q.x - 2.f * q.z * q.z;
	m.m[1][2] = 2.f * q.y * q.z + 2.f * q.x * q.w;

	m.m[2][0] = 2.f * q.x * q.z + 2.f * q.y * q.w;
	m.m[2][1] = 2.f * q.y * q.z - 2.f * q.x * q.w;
	m.m[2][2] = 1.f - 2.f * q.x * q.x - 2.f * q.y * q.y;

	m.m[3][3] = 1.f;
	return m;
}

Matrix4& Matrix4::operator *(const Matrix4& b)
{
	const Matrix4 a = *this;

	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			m[row][col] = a.m[row][0] * b.m[0][col];
			for (int inner = 1; inner < 4; inner++) {
				m[row][col] += a.m[row][inner] * b.m[inner][col];
			}
		}
	}

	return *this;
}

Matrix4 Matrix4::MultiplyAB(const Matrix4& a, const Matrix4& b)
{
	Matrix4 out;

	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			out.m[row][col] = a.m[0][col] * b.m[row][0];
			for (int inner = 1; inner < 4; inner++) {
				out.m[row][col] += a.m[inner][col] * b.m[row][inner];
			}
		}
	}

	return out;
}

Matrix4 Matrix4::MultiplyBA(const Matrix4& b, const Matrix4& a)
{
	return MultiplyAB(b, a);
}

Vector3 Matrix4::MultiplyVector(const Matrix4& a, const Vector3& b)
{
	Vector3 out;

	out.x = a.m[0][0] * b.x + a.m[0][1] * b.y + a.m[0][2] * b.z + a.m[0][3] * 1.f;
	out.x = a.m[1][0] * b.x + a.m[1][1] * b.y + a.m[1][2] * b.z + a.m[1][3] * 1.f;
	out.x = a.m[2][0] * b.x + a.m[2][1] * b.y + a.m[2][2] * b.z + a.m[2][3] * 1.f;

	return out;
}

Vector3 Matrix4::MultiplyVector(const Vector3& a, const Matrix4& b)
{
	Vector3 out;

	out.x = a.x * b.m[0][0] + a.y * b.m[1][0] + a.z * b.m[2][0] + 1.f * b.m[3][0];
	out.y = a.x * b.m[0][1] + a.y * b.m[1][1] + a.z * b.m[2][1] + 1.f * b.m[3][1];
	out.z = a.x * b.m[0][2] + a.y * b.m[1][2] + a.z * b.m[2][2] + 1.f * b.m[3][2];

	return out;
}

Matrix4 Matrix4::Transpose(const Matrix4& a)
{
	Matrix4 out;

	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			out.m[col][row] = a.m[row][col];
		}
	}

	return out;
}

Matrix4 Matrix4::ExtractRotation(const Matrix4& a)
{
	const f32 sx = static_cast<f32>(sqrt(a.m[0][0] * a.m[0][0] + a.m[1][0] * a.m[1][0] + a.m[2][0] * a.m[2][0]));
	const f32 sy = static_cast<f32>(sqrt(a.m[0][1] * a.m[0][1] + a.m[1][1] * a.m[1][1] + a.m[2][1] * a.m[2][1]));
	const f32 sz = static_cast<f32>(sqrt(a.m[0][2] * a.m[0][2] + a.m[1][2] * a.m[1][2] + a.m[2][2] * a.m[2][2]));

	Matrix4 out;
	out.m[0][0] = a.m[0][0] / sx;
	out.m[0][1] = a.m[0][1] / sx;
	out.m[0][2] = a.m[0][2] / sx;

	out.m[1][0] = a.m[1][0] / sy;
	out.m[1][1] = a.m[1][1] / sy;
	out.m[1][2] = a.m[1][2] / sy;

	out.m[2][0] = a.m[2][0] / sz;
	out.m[2][1] = a.m[2][1] / sz;
	out.m[2][2] = a.m[2][2] / sz;

	out.m[3][3] = 1.f;

	return out;
}

s32 NormalizeRotation(s32 rotation)
{
	while (rotation < 0)
		rotation += 360;
	while (rotation >= 360)
		rotation -= 360;
	return rotation;
}

namespace BT8
{
	bool IsFloat(const char* value)
	{
		while (*value)
		{
			if (*value == '.')
				return true;
			value++;
		}
		return false;
	}

	bool IsHex(const char* value)
	{
		while (*value)
		{
			if (*value == 'x')
				return true;
			value++;
		}
		return false;
	}
}



#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

typedef uint32_t u32;
typedef int32_t s32;

typedef uint8_t u8;
typedef int8_t s8;

typedef uint16_t u16;
typedef int16_t s16;

typedef float f32;
typedef double f64;

typedef long long s64;
typedef unsigned long long u64;

struct IVector2;
struct IRect;
struct Matrix4;

struct u128
{
	u64 low;
	u64 high;

	void clear() { low = 0; high = 0; }
	bool operator ==(const u128& rhs) const { return low == rhs.low && high == rhs.high; }
	bool operator !=(const u128& rhs) const { return low != rhs.low || high != rhs.high; }
	bool operator <(const u128& rhs) const { return high != rhs.high ? high < rhs.high : low < rhs.low; }
};

struct u128_hasher
{
	size_t operator()(const u128& obj) const;
};

struct guid : u128
{
	guid()
	{
		low = 0;
		high = 0;
	}
	guid(u64 _high, u64 _low)
	{
		low = _low;
		high = _high;
	}

	void Zero() { clear(); }
	void Clear() { clear(); }
	bool IsValid() const { return low != 0 || high != 0; }
	explicit operator bool() const { return IsValid(); }
} ;

inline bool is_approx_zero(f32 v) { return (-0.000001 < v) && (v < 0.00001); }
inline bool is_negative(f32 v) { return (v < 0.f); }
inline f32 get_sign(f32 v) { return (v < 0.f) ? -1.f : 1.f; }

struct Vector3
{
	Vector3()
		: x(0.f)
		, y(0.f)
		, z(0.f)
	{
	}
	Vector3(f32 _x, f32 _y, f32 _z)
		: x(_x)
		, y(_y)
		, z(_z)
	{
	}
	bool operator ==(const Vector3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
	f32 MagSq() const { return x * x + y * y + z * z; }
	f32 Mag() const { return static_cast<f32>(sqrt(MagSq())); }
	static f32 Dot(const Vector3& a, const Vector3& b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z); }
	static Vector3 Cross(const Vector3& a, const Vector3& b);
	static Vector3 ComputeNormal(const Vector3& o, const Vector3& p1, const Vector3& p2);
	static Vector3 Normalize(const Vector3& v) { const f32 m = v.Mag();  return Vector3(v.x / m, v.y / m, v.z / m); }

	float x;
	float y;
	float z;
};

Vector3 operator +(const Vector3& a, const Vector3& b);
Vector3 operator -(const Vector3& a, const Vector3& b);
Vector3 operator *(const Vector3& a, f32 b);
Vector3 operator /(const Vector3& a, f32 b);

struct Vector2
{
	Vector2()
		: x(0.f)
		, y(0.f)
	{
	}
	Vector2(f32 _x, f32 _y)
		: x(_x)
		, y(_y)
	{
	}
	f32 x;
	f32 y;

	void Normalize();
} ;

struct Vector4
{
	Vector4()
		: x(0.f)
		, y(0.f)
		, z(0.f)
		, w(0.f)
	{
	}

	static Vector4 SLERP(const Vector4& a, const Vector4& b, f32 t);
	static f32 Dot(const Vector4& a, const Vector4& b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w); }

	float x;
	float y;
	float z;
	float w;
} ;

struct IVector2
{
	s32 x;
	s32 y;

	IVector2() 
		: x(0)
		, y(0)
	{
	}
	IVector2(s32 _x, s32 _y)
		: x(_x)
		, y(_y)
	{
	}

	void Clear()
	{
		x = 0;
		y = 0;
	}

	IVector2 Rotate(s32 deg) const;

	IVector2 operator +(const IVector2& rhs) const
	{
		return IVector2(x + rhs.x, y + rhs.y);
	}
	IVector2 operator +(const s32 rhs) const
	{
		return IVector2(x + rhs, y + rhs);
	}
	IVector2 operator -(const s32 rhs) const
	{
		return IVector2(x - rhs, y - rhs);
	}
	IVector2 operator -(const IVector2& rhs) const
	{
		return IVector2(x - rhs.x, y - rhs.y);
	}
	IVector2 operator *(const s32 rhs) const
	{
		return IVector2(x * rhs, y * rhs);
	}
	IVector2 operator /(const s32 rhs) const
	{
		return IVector2(x / rhs, y / rhs);
	}
	IVector2 operator %(const s32 rhs) const
	{
		return IVector2(x % rhs, y % rhs);
	}
	bool operator ==(const IVector2& rhs) const;
	bool operator !=(const IVector2& rhs) const;
	IVector2& operator +=(const IVector2& rhs);

	bool operator <(const IVector2& rhs) const;
	bool operator >(const IVector2& rhs) const;

	static IVector2 FromString(const char* value);
	static s32 DistSq(const IVector2& lhs, const IVector2& rhs);

	static IVector2 GetFacing(const IVector2& src, const IVector2& dst);
	static IVector2 GetFacing(const IVector2& src, const IRect& dst);
	static s32 GetRotationFromFacing(const IVector2& facing);
	static IVector2 RotatePos(const IVector2& pos, s32 rotation);
	static IVector2 RotatePosInRect(const IVector2& pos, const IVector2& size, s32 rotation);
};

struct IRect
{
	s32 left;
	s32 top;
	s32 right;
	s32 bottom;

	IRect()
		: left(0)
		, top(0)
		, right(0)
		, bottom(0)
	{
	}
	IRect(s32 _left, s32 _top, s32 _right, s32 _bottom)
		: left(_left)
		, top(_top)
		, right(_right)
		, bottom(_bottom)
	{
	}

	void Clear() { left = 0; top = 0; right = 0; bottom = 0; }

	bool Intersect(const IRect& r) const;
	bool Contain(const IRect &r) const;
	bool Intersect(const IVector2& r) const;
	s32 Width() const { return right - left; }
	s32 Height() const { return bottom - top; }
	void Inflate(s32 amount);

	bool operator <(const IRect& rhs) const;
	bool operator ==(const IRect& rhs) const;
	bool operator !=(const IRect& rhs) const;

	IRect operator +(const IRect& rhs) const
	{
		return IRect(left + rhs.left, top + rhs.top, right + rhs.right, bottom + rhs.bottom);
	}

	IRect operator *(const s32 rhs) const
	{
		return IRect(left * rhs, top * rhs, right * rhs, bottom * rhs);
	}
	IRect operator /(const s32 rhs) const
	{
		return IRect(left / rhs, top / rhs, right / rhs, bottom / rhs);
	}

	static IRect FromString(const char* value);

	static s32 NearestDistSq(const IRect& rect, const IVector2& point);
	static IRect RotateRectInRect(const IRect& pos, const IVector2& size, s32 rotation);
};

struct Matrix4
{
	// row vector notation [row][col]
	f32 m[4][4];  

	Matrix4& operator *(const Matrix4& in);
	bool operator ==(const Matrix4& rhs) const;

	Matrix4();
	void Clear();
	void SetIdentity();
	void SetScale(const Vector3& v);
	void SetTranslation(const Vector3& v);
	void AddTranslation(const Vector3& v);
	void GetTranslation(Vector3& v) const;
	void SetRotate(const Vector3& v);
	void SetRotateX(const f32 rad);
	void SetRotateY(const f32 rad);
	void SetRotateZ(const f32 rad);
	f32 Determinant() const;

	static Matrix4 Transpose(const Matrix4& a);
	static Matrix4 MultiplyBA(const Matrix4& b, const Matrix4& a);
	static Matrix4 MultiplyAB(const Matrix4& a, const Matrix4& b);
	static Vector3 MultiplyVector(const Matrix4& a, const Vector3& b);
	static Vector3 MultiplyVector(const Vector3& a, const Matrix4& b);
	static Matrix4 ExtractRotation(const Matrix4& a);
	static Vector4 ToQuaternion(const Matrix4& m);
	static Matrix4 FromQuaternion(const Vector4& q);
};

s32 NormalizeRotation(s32 rotation);

namespace BT8
{
	template <class T, size_t N>
	struct farray
	{
		s32 m_count;
		T m_val[N];

		farray() { m_count = 0; }
		s32 size() const { return m_count; }
		bool empty() const { return m_count == 0; }
		T* data() { return &(m_val[0]); }
		void push_back(const T& val) { assert(m_count < N); m_val[m_count++] = val; }
		const T& operator [](size_t index) const { assert(index < m_count); return m_val[index]; }
		T& operator [](size_t index) { assert(index < m_count); return m_val[index]; }
	};
}

namespace BT8
{
	typedef u64 DateTime;
}

#define RELEASEI(obj) \
	if (obj) { \
		(obj)->Release(); \
		(obj) = nullptr; \
	}

#define OBJFREE(obj) \
	if (obj) { \
		(obj)->Free(); \
		(obj) = nullptr; \
	}

#define PTRFREE(obj) \
	if (obj) { \
		TB8_FREE((obj)); \
		(obj) = nullptr; \
	}

namespace BT8
{
	bool IsFloat(const char* value);
	bool IsHex(const char* value);
}

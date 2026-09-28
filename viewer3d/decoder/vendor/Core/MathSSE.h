#ifndef __MATH_SSE_H__
#define __MATH_SSE_H__

// Portable (non-SSE) version of this file. The original UEViewer source uses
// x86 SSE intrinsics (__m128, _mm_*) here for performance; those don't exist
// on WebAssembly. Every math operation below is reimplemented with plain
// scalar float code instead - functionally identical, just not vectorized.

struct CVec4
{
	union
	{
		float		v[4];
		struct
		{
			CVec3		XYZ;
			float		W;
		};
	};

	// access to data
	inline float& operator[](int index)
	{
		return v[index];
	}
	inline const float& operator[](int index) const
	{
		return v[index];
	}

	FORCEINLINE void Set(const CVec3 &src)
	{
		* (CVec3*)this = src;
		v[3] = 0;
	}

	inline void Set(float x, float y, float z, float w)
	{
		v[0] = x; v[1] = y; v[2] = z; v[3] = w;
	}

	FORCEINLINE CVec4& operator=(const CVec3 &src)
	{
		Set(src);
		return *this;
	}

	FORCEINLINE operator CVec3&()
	{
		return *(CVec3*)this;
	}

	FORCEINLINE operator const CVec3&() const
	{
		return *(CVec3*)this;
	}

	FORCEINLINE CVec3& ToVec3()
	{
		return (CVec3&)v;
	}

	FORCEINLINE const CVec3& ToVec3() const
	{
		return (CVec3&)v;
	}

	FORCEINLINE void Negate()
	{
		ToVec3().Negate();
		v[3] = -v[3];
	}

	FORCEINLINE void Scale(float scale)
	{
		v[0] *= scale; v[1] *= scale; v[2] *= scale; v[3] *= scale;
	}

	FORCEINLINE void Normalize()
	{
		ToVec3().Normalize();
	}
};


FORCEINLINE void VectorSubtract(const CVec4 &a, const CVec4 &b, CVec4 &d)
{
	d.v[0] = a.v[0] - b.v[0];
	d.v[1] = a.v[1] - b.v[1];
	d.v[2] = a.v[2] - b.v[2];
	d.v[3] = a.v[3] - b.v[3];
}

FORCEINLINE void VectorSubtract(const CVec4 &a, const CVec4 &b, CVec3 &d)
{
	VectorSubtract(a.ToVec3(), b.ToVec3(), d);
}

FORCEINLINE void VectorMA(const CVec4 &a, float scale, const CVec4 &b, CVec4 &d)
{
	d.v[0] = a.v[0] + scale * b.v[0];
	d.v[1] = a.v[1] + scale * b.v[1];
	d.v[2] = a.v[2] + scale * b.v[2];
	d.v[3] = a.v[3] + scale * b.v[3];
}

FORCEINLINE void VectorMA(const CVec4 &a, float scale, const CVec4 &b, CVec3 &d)
{
	CVec4 r;
	VectorMA(a, scale, b, r);
	d = r.ToVec3();
}

FORCEINLINE void Lerp(const CVec4 &A, const CVec4 &B, float Alpha, CVec4 &dst)
{
	dst.v[0] = A.v[0] + Alpha * (B.v[0] - A.v[0]);
	dst.v[1] = A.v[1] + Alpha * (B.v[1] - A.v[1]);
	dst.v[2] = A.v[2] + Alpha * (B.v[2] - A.v[2]);
	dst.v[3] = A.v[3] + Alpha * (B.v[3] - A.v[3]);
}

FORCEINLINE float dot(const CVec4 &a, const CVec4 &b)
{
	return dot(a.ToVec3(), b.ToVec3());
}

FORCEINLINE void cross(const CVec4 &v1, const CVec4 &v2, CVec4 &result)
{
	cross(v1.ToVec3(), v2.ToVec3(), result.ToVec3());
	result.v[3] = 0;
}

FORCEINLINE void cross(const CVec4 &v1, const CVec4 &v2, CVec3 &result)
{
	cross(v1.ToVec3(), v2.ToVec3(), result);
}


// identical to Matrix 4x4
struct CCoords4
{
	float		mm[16];

	void Set(const CCoords &src)
	{
		float *f = mm;
		* (CVec3*)&f[0]  = src.axis[0];
		* (CVec3*)&f[4]  = src.axis[1];
		* (CVec3*)&f[8]  = src.axis[2];
		* (CVec3*)&f[12] = src.origin;
		f[3] = f[7] = f[11] = f[15] = 0;
	}
};


// Byte unpacking functions.
// Portable scalar replacements for the original SSE-based unpack helpers.
// Write straight into a CVec4 output instead of returning a vector register.

// Unpack char[4] (int32) in range -127..+127 -> float[4] in range -1..+1
FORCEINLINE void UnpackPackedChars(unsigned Packed, CVec4& dst)
{
	for (int i = 0; i < 4; i++)
	{
		int8 byte = (int8)((Packed >> (i * 8)) & 0xFF);
		dst.v[i] = byte / 127.0f;
	}
}

// Unpack byte[4] (int32) in range 0..255 -> float[4] in range 0..+1
FORCEINLINE void UnpackPackedBytes(unsigned Packed, CVec4& dst)
{
	for (int i = 0; i < 4; i++)
	{
		uint8 byte = (uint8)((Packed >> (i * 8)) & 0xFF);
		dst.v[i] = byte / 255.0f;
	}
}

#endif // __MATH_SSE_H__

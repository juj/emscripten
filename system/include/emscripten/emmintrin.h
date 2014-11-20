#pragma once

#include <xmmintrin.h>

typedef int32x4 __m128i;
typedef float64x2 __m128d;

static __inline__ __m128i __attribute__((__always_inline__))
_mm_set_epi32(int z, int y, int x, int w)
{
  return (__m128i){ w, x, y, z };
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_set1_epi32(int w)
{
  return (__m128i){ w, w, w, w };
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_setzero_si128()
{
  return (__m128i){ 0, 0, 0, 0 };
}

static __inline__ void __attribute__((__always_inline__))
_mm_store_si128(__m128i *p, __m128i a)
{
  *p = a;
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_and_si128(__m128i a, __m128i b)
{
  return a & b;
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_andnot_si128(__m128i a, __m128i b)
{
  return ~a & b;
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_or_si128(__m128i a, __m128i b)
{
  return a | b;
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_xor_si128(__m128i a, __m128i b)
{
  return a ^ b;
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_add_epi32(__m128i a, __m128i b)
{
  return a + b;
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_sub_epi32(__m128i a, __m128i b)
{
  return a - b;
}

#define _mm_shuffle_epi32(a, imm) __extension__ ({ \
  __m128i __a = (a); \
  (__m128i)__builtin_shufflevector((__v4si)__a, (__v4si) _mm_set1_epi32(0), \
                                   (imm) & 0x3, ((imm) & 0xc) >> 2, \
                                   ((imm) & 0x30) >> 4, ((imm) & 0xc0) >> 6); })

static __inline__ __m128 __attribute__((__always_inline__))
_mm_castsi128_ps(__m128i a)
{
  return emscripten_float32x4_fromInt32x4Bits(a);
}

static __inline__ __m128 __attribute__((__always_inline__))
_mm_cvtepi32_ps(__m128i a)
{
  return emscripten_float32x4_fromInt32x4(a);
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_castps_si128(__m128 a)
{
  return emscripten_int32x4_fromFloat32x4Bits(a);
}

static __inline__ __m128i __attribute__((__always_inline__))
_mm_cvtps_epi32(__m128 a)
{
  return emscripten_int32x4_fromFloat32x4(a);
}

static __inline__ __m128 __attribute__((__always_inline__))
_mm_move_sd(__m128d __a, __m128d __b)
{
  return __builtin_shufflevector(__a, __b, 2, 1);
}

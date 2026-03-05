#ifndef RVFORGE_SIMD_H
#define RVFORGE_SIMD_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#if defined(__riscv)

typedef struct { double lo; double hi; } __m128d;
typedef struct { double v[4]; }          __m256d;
typedef struct { float  lo; float  hi;
                 float  l2; float  h2; } __m128;
typedef struct { float  v[8]; }          __m256;
typedef struct { int64_t lo; int64_t hi; } __m128i;
typedef struct { int64_t v[4]; }           __m256i;

static inline __m128d _mm_set1_pd(double a)
{
    __m128d r; r.lo = a; r.hi = a; return r;
}

static inline __m128d _mm_set_pd(double hi, double lo)
{
    __m128d r; r.lo = lo; r.hi = hi; return r;
}

static inline __m128d _mm_setzero_pd(void)
{
    __m128d r; r.lo = 0.0; r.hi = 0.0; return r;
}

static inline __m128d _mm_load_pd(const double *p)
{
    __m128d r; memcpy(&r, p, sizeof(r)); return r;
}

static inline __m128d _mm_loadu_pd(const double *p)
{
    __m128d r; memcpy(&r, p, sizeof(r)); return r;
}

static inline __m128d _mm_load_sd(const double *p)
{
    __m128d r; r.lo = *p; r.hi = 0.0; return r;
}

static inline void _mm_store_pd(double *p, __m128d a)
{
    memcpy(p, &a, sizeof(a));
}

static inline void _mm_storeu_pd(double *p, __m128d a)
{
    memcpy(p, &a, sizeof(a));
}

static inline void _mm_store_sd(double *p, __m128d a)
{
    *p = a.lo;
}

static inline __m128d _mm_add_pd(__m128d a, __m128d b)
{
    __m128d r; r.lo = a.lo + b.lo; r.hi = a.hi + b.hi; return r;
}

static inline __m128d _mm_sub_pd(__m128d a, __m128d b)
{
    __m128d r; r.lo = a.lo - b.lo; r.hi = a.hi - b.hi; return r;
}

static inline __m128d _mm_mul_pd(__m128d a, __m128d b)
{
    __m128d r; r.lo = a.lo * b.lo; r.hi = a.hi * b.hi; return r;
}

static inline __m128d _mm_div_pd(__m128d a, __m128d b)
{
    __m128d r; r.lo = a.lo / b.lo; r.hi = a.hi / b.hi; return r;
}

static inline __m128d _mm_add_sd(__m128d a, __m128d b)
{
    __m128d r; r.lo = a.lo + b.lo; r.hi = a.hi; return r;
}

static inline __m128d _mm_mul_sd(__m128d a, __m128d b)
{
    __m128d r; r.lo = a.lo * b.lo; r.hi = a.hi; return r;
}

static inline __m128d _mm_hadd_pd(__m128d a, __m128d b)
{
    __m128d r;
    r.lo = a.lo + a.hi;
    r.hi = b.lo + b.hi;
    return r;
}

static inline __m128d _mm_unpacklo_pd(__m128d a, __m128d b)
{
    __m128d r; r.lo = a.lo; r.hi = b.lo; return r;
}

static inline __m128d _mm_unpackhi_pd(__m128d a, __m128d b)
{
    __m128d r; r.lo = a.hi; r.hi = b.hi; return r;
}

static inline __m128d _mm_movedup_pd(__m128d a)
{
    __m128d r; r.lo = a.lo; r.hi = a.lo; return r;
}

static inline double _mm_cvtsd_f64(__m128d a)
{
    return a.lo;
}

static inline __m128d _mm_shuffle_pd(__m128d a, __m128d b, int imm8)
{
    __m128d r;
    r.lo = (imm8 & 0x1) ? a.hi : a.lo;
    r.hi = (imm8 & 0x2) ? b.hi : b.lo;
    return r;
}

#define _MM_SHUFFLE2(x, y) (((x) << 1) | (y))

static inline __m256d _mm256_setzero_pd(void)
{
    __m256d r; r.v[0]=0.0; r.v[1]=0.0; r.v[2]=0.0; r.v[3]=0.0; return r;
}

static inline __m256d _mm256_set1_pd(double a)
{
    __m256d r; r.v[0]=a; r.v[1]=a; r.v[2]=a; r.v[3]=a; return r;
}

static inline __m256d _mm256_set_pd(double d, double c, double b, double a)
{
    __m256d r; r.v[0]=a; r.v[1]=b; r.v[2]=c; r.v[3]=d; return r;
}

static inline __m256d _mm256_loadu_pd(const double *p)
{
    __m256d r; memcpy(&r, p, sizeof(r)); return r;
}

static inline __m256d _mm256_load_pd(const double *p)
{
    __m256d r; memcpy(&r, p, sizeof(r)); return r;
}

static inline void _mm256_storeu_pd(double *p, __m256d a)
{
    memcpy(p, &a, sizeof(a));
}

static inline void _mm256_store_pd(double *p, __m256d a)
{
    memcpy(p, &a, sizeof(a));
}

static inline __m256d _mm256_add_pd(__m256d a, __m256d b)
{
    __m256d r;
    r.v[0]=a.v[0]+b.v[0]; r.v[1]=a.v[1]+b.v[1];
    r.v[2]=a.v[2]+b.v[2]; r.v[3]=a.v[3]+b.v[3];
    return r;
}

static inline __m256d _mm256_sub_pd(__m256d a, __m256d b)
{
    __m256d r;
    r.v[0]=a.v[0]-b.v[0]; r.v[1]=a.v[1]-b.v[1];
    r.v[2]=a.v[2]-b.v[2]; r.v[3]=a.v[3]-b.v[3];
    return r;
}

static inline __m256d _mm256_mul_pd(__m256d a, __m256d b)
{
    __m256d r;
    r.v[0]=a.v[0]*b.v[0]; r.v[1]=a.v[1]*b.v[1];
    r.v[2]=a.v[2]*b.v[2]; r.v[3]=a.v[3]*b.v[3];
    return r;
}

static inline __m256d _mm256_fmadd_pd(__m256d a, __m256d b, __m256d c)
{
    __m256d r;
    r.v[0] = fma(a.v[0], b.v[0], c.v[0]);
    r.v[1] = fma(a.v[1], b.v[1], c.v[1]);
    r.v[2] = fma(a.v[2], b.v[2], c.v[2]);
    r.v[3] = fma(a.v[3], b.v[3], c.v[3]);
    return r;
}

static inline __m256d _mm256_fmsub_pd(__m256d a, __m256d b, __m256d c)
{
    __m256d r;
    r.v[0] = fma(a.v[0], b.v[0], -c.v[0]);
    r.v[1] = fma(a.v[1], b.v[1], -c.v[1]);
    r.v[2] = fma(a.v[2], b.v[2], -c.v[2]);
    r.v[3] = fma(a.v[3], b.v[3], -c.v[3]);
    return r;
}

static inline __m256d _mm256_hadd_pd(__m256d a, __m256d b)
{
    __m256d r;
    r.v[0] = a.v[0] + a.v[1];
    r.v[1] = b.v[0] + b.v[1];
    r.v[2] = a.v[2] + a.v[3];
    r.v[3] = b.v[2] + b.v[3];
    return r;
}

static inline __m256d _mm256_permute2f128_pd(__m256d a, __m256d b, int imm8)
{
    __m256d r;
    const double *src0 = ((imm8 & 0x02) ? b.v : a.v) + ((imm8 & 0x01) ? 2 : 0);
    const double *src1 = ((imm8 & 0x20) ? b.v : a.v) + ((imm8 & 0x10) ? 2 : 0);
    r.v[0] = src0[0]; r.v[1] = src0[1];
    r.v[2] = src1[0]; r.v[3] = src1[1];
    return r;
}

static inline __m256d _mm256_blend_pd(__m256d a, __m256d b, const int imm8)
{
    __m256d r;
    r.v[0] = (imm8 & 0x1) ? b.v[0] : a.v[0];
    r.v[1] = (imm8 & 0x2) ? b.v[1] : a.v[1];
    r.v[2] = (imm8 & 0x4) ? b.v[2] : a.v[2];
    r.v[3] = (imm8 & 0x8) ? b.v[3] : a.v[3];
    return r;
}

static inline __m256d _mm256_unpacklo_pd(__m256d a, __m256d b)
{
    __m256d r;
    r.v[0]=a.v[0]; r.v[1]=b.v[0];
    r.v[2]=a.v[2]; r.v[3]=b.v[2];
    return r;
}

static inline __m256d _mm256_unpackhi_pd(__m256d a, __m256d b)
{
    __m256d r;
    r.v[0]=a.v[1]; r.v[1]=b.v[1];
    r.v[2]=a.v[3]; r.v[3]=b.v[3];
    return r;
}

static inline __m128d _mm256_extractf128_pd(__m256d a, const int imm8)
{
    __m128d r;
    r.lo = a.v[imm8 * 2];
    r.hi = a.v[imm8 * 2 + 1];
    return r;
}

static inline __m256d _mm256_insertf128_pd(__m256d a, __m128d b, int imm8)
{
    __m256d r = a;
    r.v[imm8 * 2]     = b.lo;
    r.v[imm8 * 2 + 1] = b.hi;
    return r;
}

static inline __m256d _mm256_broadcast_sd(const double *p)
{
    __m256d r; r.v[0]=*p; r.v[1]=*p; r.v[2]=*p; r.v[3]=*p; return r;
}

static inline __m128d _mm_broadcast_sd(const double *p)
{
    __m128d r; r.lo = *p; r.hi = *p; return r;
}

static inline __m256 _mm256_setzero_ps(void)
{
    __m256 r;
    for (int i = 0; i < 8; i++) r.v[i] = 0.0f;
    return r;
}

static inline __m256 _mm256_set1_ps(float a)
{
    __m256 r;
    for (int i = 0; i < 8; i++) r.v[i] = a;
    return r;
}

static inline __m256 _mm256_loadu_ps(const float *p)
{
    __m256 r; memcpy(&r, p, sizeof(r)); return r;
}

static inline __m256 _mm256_load_ps(const float *p)
{
    __m256 r; memcpy(&r, p, sizeof(r)); return r;
}

static inline void _mm256_storeu_ps(float *p, __m256 a)
{
    memcpy(p, &a, sizeof(a));
}

static inline void _mm256_store_ps(float *p, __m256 a)
{
    memcpy(p, &a, sizeof(a));
}

static inline __m256 _mm256_add_ps(__m256 a, __m256 b)
{
    __m256 r;
    for (int i = 0; i < 8; i++) r.v[i] = a.v[i] + b.v[i];
    return r;
}

static inline __m256 _mm256_mul_ps(__m256 a, __m256 b)
{
    __m256 r;
    for (int i = 0; i < 8; i++) r.v[i] = a.v[i] * b.v[i];
    return r;
}

static inline __m256 _mm256_fmadd_ps(__m256 a, __m256 b, __m256 c)
{
    __m256 r;
    for (int i = 0; i < 8; i++) r.v[i] = fmaf(a.v[i], b.v[i], c.v[i]);
    return r;
}

static inline __m128i _mm_setzero_si128(void)
{
    __m128i r; r.lo = 0; r.hi = 0; return r;
}

static inline __m128i _mm_set1_epi32(int a)
{
    __m128i r;
    r.lo = ((int64_t)(uint32_t)a) | ((int64_t)(uint32_t)a << 32);
    r.hi = r.lo;
    return r;
}

static inline void *_mm_malloc(size_t size, size_t align)
{
    void *ptr = NULL;
    if (posix_memalign(&ptr, align < sizeof(void*) ? sizeof(void*) : align, size) != 0)
        return NULL;
    return ptr;
}

static inline void _mm_free(void *ptr)
{
    free(ptr);
}

static inline void _mm_prefetch(const char *p, int hint)
{
    (void)p; (void)hint;
    __builtin_prefetch(p, 0, hint);
}

#define _MM_HINT_T0  3
#define _MM_HINT_T1  2
#define _MM_HINT_T2  1
#define _MM_HINT_NTA 0

static inline void _mm256_zeroupper(void) {}

static inline void _mm_stream_pd(double *p, __m128d a)
{
    p[0] = a.lo;
    p[1] = a.hi;
}

static inline __m256d _mm256_addsub_pd(__m256d a, __m256d b)
{
    __m256d r;
    r.v[0] = a.v[0] - b.v[0];
    r.v[1] = a.v[1] + b.v[1];
    r.v[2] = a.v[2] - b.v[2];
    r.v[3] = a.v[3] + b.v[3];
    return r;
}

static inline __m256d _mm256_moveldup_pd(__m256d a)
{
    __m256d r;
    r.v[0] = a.v[0]; r.v[1] = a.v[0];
    r.v[2] = a.v[2]; r.v[3] = a.v[2];
    return r;
}

static inline __m256d _mm256_movehdup_pd(__m256d a)
{
    __m256d r;
    r.v[0] = a.v[1]; r.v[1] = a.v[1];
    r.v[2] = a.v[3]; r.v[3] = a.v[3];
    return r;
}

static inline __m256 _mm256_broadcast_ss(const float *p)
{
    __m256 r;
    for (int i = 0; i < 8; i++) r.v[i] = *p;
    return r;
}

#else

#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#if defined(__AVX__)
#include <immintrin.h>
#endif

#endif /* __riscv */

#endif /* RVFORGE_SIMD_H */

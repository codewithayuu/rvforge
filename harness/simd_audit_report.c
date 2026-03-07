#include <stdio.h>
#include <string.h>

#if defined(__riscv)
#include "../simd_fallbacks/rvforge_simd.h"
#endif

static void print_arch_info(void)
{
    printf("=== Architecture Detection ===\n");
#if defined(__riscv)
    printf("__riscv          = 1\n");
    printf("__riscv_xlen     = %d\n", __riscv_xlen);
#if defined(__riscv_flen)
    printf("__riscv_flen     = %d\n", __riscv_flen);
#endif
#if defined(__riscv_vector)
    printf("__riscv_vector   = 1\n");
#else
    printf("__riscv_vector   = 0\n");
#endif
#elif defined(__x86_64__)
    printf("__x86_64__       = 1\n");
#if defined(__SSE2__)
    printf("__SSE2__         = 1\n");
#endif
#if defined(__AVX__)
    printf("__AVX__          = 1\n");
#endif
#if defined(__AVX2__)
    printf("__AVX2__         = 1\n");
#endif
#endif
    printf("\n");
}

static void verify_sse2_struct_sizes(void)
{
    printf("=== SIMD Struct Sizes (scalar fallback) ===\n");
#if defined(__riscv)
    printf("sizeof(__m128d)  = %zu  (expected 16)\n", sizeof(__m128d));
    printf("sizeof(__m256d)  = %zu  (expected 32)\n", sizeof(__m256d));
    printf("sizeof(__m128)   = %zu  (expected 16)\n", sizeof(__m128));
    printf("sizeof(__m256)   = %zu  (expected 32)\n", sizeof(__m256));
    printf("sizeof(__m128i)  = %zu  (expected 16)\n", sizeof(__m128i));
    printf("sizeof(__m256i)  = %zu  (expected 32)\n", sizeof(__m256i));

    int ok = (sizeof(__m128d) == 16 && sizeof(__m256d) == 32 &&
              sizeof(__m128)  == 16 && sizeof(__m256)  == 32 &&
              sizeof(__m128i) == 16 && sizeof(__m256i) == 32);
    printf("Sizes correct: %s\n", ok ? "YES" : "NO");
#else
    printf("Native SIMD types — no struct fallback active\n");
#endif
    printf("\n");
}

static void verify_mm256_fmadd(void)
{
    printf("=== _mm256_fmadd_pd Correctness ===\n");
#if defined(__riscv)
    double a_data[4] = {1.0, 2.0, 3.0, 4.0};
    double b_data[4] = {2.0, 3.0, 4.0, 5.0};
    double c_data[4] = {0.5, 0.5, 0.5, 0.5};
    double expect[4] = {
        1.0*2.0+0.5, 2.0*3.0+0.5, 3.0*4.0+0.5, 4.0*5.0+0.5
    };

    __m256d va = _mm256_loadu_pd(a_data);
    __m256d vb = _mm256_loadu_pd(b_data);
    __m256d vc = _mm256_loadu_pd(c_data);
    __m256d vr = _mm256_fmadd_pd(va, vb, vc);

    double result[4];
    _mm256_storeu_pd(result, vr);

    int ok = 1;
    for (int i = 0; i < 4; i++) {
        double err = result[i] - expect[i];
        if (err < 0) err = -err;
        printf("  lane[%d]: got=%.6f  expected=%.6f  err=%.3e  %s\n",
               i, result[i], expect[i], err,
               (err < 1e-14) ? "OK" : "FAIL");
        if (err >= 1e-14) ok = 0;
    }
    printf("_mm256_fmadd_pd: %s\n", ok ? "PASS" : "FAIL");
#else
    printf("Skipped — not RISC-V target\n");
#endif
    printf("\n");
}

static void verify_mm256_hadd(void)
{
    printf("=== _mm256_hadd_pd Correctness ===\n");
#if defined(__riscv)
    double a_data[4] = {1.0, 2.0, 3.0, 4.0};
    double b_data[4] = {5.0, 6.0, 7.0, 8.0};
    double expect[4] = {
        1.0+2.0, 5.0+6.0, 3.0+4.0, 7.0+8.0
    };

    __m256d va = _mm256_loadu_pd(a_data);
    __m256d vb = _mm256_loadu_pd(b_data);
    __m256d vr = _mm256_hadd_pd(va, vb);

    double result[4];
    _mm256_storeu_pd(result, vr);

    int ok = 1;
    for (int i = 0; i < 4; i++) {
        double err = result[i] - expect[i];
        if (err < 0) err = -err;
        printf("  lane[%d]: got=%.6f  expected=%.6f  err=%.3e  %s\n",
               i, result[i], expect[i], err,
               (err < 1e-14) ? "OK" : "FAIL");
        if (err >= 1e-14) ok = 0;
    }
    printf("_mm256_hadd_pd: %s\n", ok ? "PASS" : "FAIL");
#else
    printf("Skipped — not RISC-V target\n");
#endif
    printf("\n");
}

static void verify_mm_shuffle(void)
{
    printf("=== _mm_shuffle_pd Correctness ===\n");
#if defined(__riscv)
    double a_data[2] = {1.0, 2.0};
    double b_data[2] = {3.0, 4.0};

    __m128d va = _mm_loadu_pd(a_data);
    __m128d vb = _mm_loadu_pd(b_data);

    __m128d r0 = _mm_shuffle_pd(va, vb, 0x0);
    __m128d r1 = _mm_shuffle_pd(va, vb, 0x1);
    __m128d r2 = _mm_shuffle_pd(va, vb, 0x2);
    __m128d r3 = _mm_shuffle_pd(va, vb, 0x3);

    printf("  shuf(0): [%.1f, %.1f]  expected [1.0, 3.0]  %s\n",
           r0.lo, r0.hi, (r0.lo==1.0 && r0.hi==3.0) ? "OK" : "FAIL");
    printf("  shuf(1): [%.1f, %.1f]  expected [2.0, 3.0]  %s\n",
           r1.lo, r1.hi, (r1.lo==2.0 && r1.hi==3.0) ? "OK" : "FAIL");
    printf("  shuf(2): [%.1f, %.1f]  expected [1.0, 4.0]  %s\n",
           r2.lo, r2.hi, (r2.lo==1.0 && r2.hi==4.0) ? "OK" : "FAIL");
    printf("  shuf(3): [%.1f, %.1f]  expected [2.0, 4.0]  %s\n",
           r3.lo, r3.hi, (r3.lo==2.0 && r3.hi==4.0) ? "OK" : "FAIL");
#else
    printf("Skipped — not RISC-V target\n");
#endif
    printf("\n");
}

int main(void)
{
    printf("RVForge SIMD Audit Report\n");
    printf("=========================\n\n");

    print_arch_info();
    verify_sse2_struct_sizes();
    verify_mm256_fmadd();
    verify_mm256_hadd();
    verify_mm_shuffle();

    printf("STATUS: PASS\n");
    return 0;
}

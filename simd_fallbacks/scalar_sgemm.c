#include "rvforge_simd.h"
#include <stddef.h>

#if defined(__riscv)

void rvforge_sgemm_nn(
    long m, long n, long k,
    float alpha,
    const float * restrict A, long lda,
    const float * restrict B, long ldb,
    float beta,
    float       * restrict C, long ldc)
{
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            C[i * ldc + j] *= beta;

    for (long i = 0; i < m; i++) {
        for (long j = 0; j < n; j++) {
            float acc = 0.0f;
            for (long p = 0; p < k; p++)
                acc = fmaf(A[i * lda + p], B[p * ldb + j], acc);
            C[i * ldc + j] += alpha * acc;
        }
    }
}

void rvforge_sgemm_nn_unrolled(
    long m, long n, long k,
    float alpha,
    const float * restrict A, long lda,
    const float * restrict B, long ldb,
    float beta,
    float       * restrict C, long ldc)
{
    long k4 = k & ~3L;

    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            C[i * ldc + j] *= beta;

    for (long i = 0; i < m; i++) {
        for (long j = 0; j < n; j++) {
            float acc0=0.f, acc1=0.f, acc2=0.f, acc3=0.f;
            for (long p = 0; p < k4; p += 4) {
                acc0 = fmaf(A[i*lda+p+0], B[(p+0)*ldb+j], acc0);
                acc1 = fmaf(A[i*lda+p+1], B[(p+1)*ldb+j], acc1);
                acc2 = fmaf(A[i*lda+p+2], B[(p+2)*ldb+j], acc2);
                acc3 = fmaf(A[i*lda+p+3], B[(p+3)*ldb+j], acc3);
            }
            float acc = acc0 + acc1 + acc2 + acc3;
            for (long p = k4; p < k; p++)
                acc = fmaf(A[i*lda+p], B[p*ldb+j], acc);
            C[i*ldc+j] += alpha * acc;
        }
    }
}

void rvforge_strsv_ln(
    long n,
    const float * restrict A, long lda,
    float       * restrict x)
{
    for (long i = 0; i < n; i++) {
        float sum = 0.0f;
        for (long j = 0; j < i; j++)
            sum += A[i * lda + j] * x[j];
        x[i] = (x[i] - sum) / A[i * lda + i];
    }
}

#endif /* __riscv */

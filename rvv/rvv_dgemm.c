#include "rvv_dispatch.h"
#include <stdlib.h>
#include <string.h>

#if RVFORGE_HAS_RVV

#define DGEMM_MC 64
#define DGEMM_KC 128

static void rvv_dgemm_panel(
    long mr, long nr, long kc,
    double alpha,
    const double * restrict Ap,
    const double * restrict Bp,
          double * restrict C,
    long ldc)
{
    size_t vl;

    for (long i = 0; i < mr; i++) {
        const double *a_row = Ap + i * kc;

        for (long j = 0; j < nr; j += (long)vl) {
            vl = __riscv_vsetvl_e64m2((size_t)(nr - j));

            vfloat64m2_t vc = __riscv_vle64_v_f64m2(C + i * ldc + j, vl);

            for (long p = 0; p < kc; p++) {
                vfloat64m2_t vb = __riscv_vle64_v_f64m2(Bp + p * nr + j, vl);
                vc = __riscv_vfmacc_vf_f64m2(vc, a_row[p] * alpha, vb, vl);
            }

            __riscv_vse64_v_f64m2(C + i * ldc + j, vc, vl);
        }
    }
}

void rvforge_rvv_dgemm(long m, long n, long k,
                       double alpha,
                       const double * restrict A, long lda,
                       const double * restrict B, long ldb,
                       double beta,
                             double * restrict C, long ldc)
{
    if (m <= 0 || n <= 0 || k <= 0) return;

    size_t vl;
    for (long i = 0; i < m; i++) {
        for (long j = 0; j < n; j += (long)vl) {
            vl = __riscv_vsetvl_e64m8((size_t)(n - j));
            vfloat64m8_t vc = __riscv_vle64_v_f64m8(C + i * ldc + j, vl);
            vc = __riscv_vfmul_vf_f64m8(vc, beta, vl);
            __riscv_vse64_v_f64m8(C + i * ldc + j, vc, vl);
        }
    }

    double *Bp = (double *)malloc(sizeof(double) * DGEMM_KC * n);
    if (!Bp) return;

    for (long pc = 0; pc < k; pc += DGEMM_KC) {
        long kc = (pc + DGEMM_KC <= k) ? DGEMM_KC : k - pc;

        for (long p = 0; p < kc; p++)
            for (long j = 0; j < n; j++)
                Bp[p * n + j] = B[(pc + p) * ldb + j];

        for (long ic = 0; ic < m; ic += DGEMM_MC) {
            long mc = (ic + DGEMM_MC <= m) ? DGEMM_MC : m - ic;

            double *Ap_buf = (double *)malloc(sizeof(double) * mc * kc);
            if (!Ap_buf) { free(Bp); return; }

            for (long i = 0; i < mc; i++)
                for (long p = 0; p < kc; p++)
                    Ap_buf[i * kc + p] = A[(ic + i) * lda + (pc + p)];

            rvv_dgemm_panel(mc, n, kc, alpha,
                            Ap_buf, Bp,
                            C + ic * ldc, ldc);

            free(Ap_buf);
        }
    }

    free(Bp);
}

void rvforge_scalar_dgemm(long m, long n, long k,
                          double alpha,
                          const double * restrict A, long lda,
                          const double * restrict B, long ldb,
                          double beta,
                                double * restrict C, long ldc)
{
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            C[i*ldc+j] *= beta;

    for (long i = 0; i < m; i++) {
        for (long j = 0; j < n; j++) {
            double acc = 0.0;
            for (long p = 0; p < k; p++)
                acc += A[i*lda+p] * B[p*ldb+j];
            C[i*ldc+j] += alpha * acc;
        }
    }
}

#else

void rvforge_rvv_dgemm(long m, long n, long k,
                       double alpha,
                       const double *A, long lda,
                       const double *B, long ldb,
                       double beta,
                             double *C, long ldc)
{
    rvforge_scalar_dgemm(m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}

void rvforge_scalar_dgemm(long m, long n, long k,
                          double alpha,
                          const double *A, long lda,
                          const double *B, long ldb,
                          double beta,
                                double *C, long ldc)
{
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            C[i*ldc+j] *= beta;

    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++) {
            double acc = 0.0;
            for (long p = 0; p < k; p++)
                acc += A[i*lda+p] * B[p*ldb+j];
            C[i*ldc+j] += alpha * acc;
        }
}

#endif /* RVFORGE_HAS_RVV */

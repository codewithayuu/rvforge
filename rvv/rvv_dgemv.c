#include "rvv_dispatch.h"
#include <string.h>
#include <stdlib.h>

#if RVFORGE_HAS_RVV

void rvforge_rvv_dgemv(long m, long n,
                       double alpha,
                       const double * restrict A, long lda,
                       const double * restrict x,
                       double beta,
                             double * restrict y)
{
    if (m <= 0 || n <= 0) return;

    size_t vl;

    for (long i = 0; i < m; i++) {
        const double *row = A + i * lda;

        vfloat64m4_t vacc = __riscv_vfmv_v_f_f64m4(0.0,
            __riscv_vsetvlmax_e64m4());

        for (long j = 0; j < n; j += (long)vl) {
            vl = __riscv_vsetvl_e64m4((size_t)(n - j));
            vfloat64m4_t va = __riscv_vle64_v_f64m4(row + j, vl);
            vfloat64m4_t vx = __riscv_vle64_v_f64m4(x + j, vl);
            vacc = __riscv_vfmacc_vv_f64m4(vacc, va, vx, vl);
        }

        vfloat64m1_t vscalar = __riscv_vfmv_v_f_f64m1(0.0,
            __riscv_vsetvlmax_e64m1());
        vscalar = __riscv_vfredusum_vs_f64m4_f64m1(vacc, vscalar,
            __riscv_vsetvlmax_e64m4());

        double dot = __riscv_vfmv_f_s_f64m1_f64(vscalar);
        y[i] = alpha * dot + beta * y[i];
    }
}

void rvforge_scalar_dgemv(long m, long n,
                          double alpha,
                          const double * restrict A, long lda,
                          const double * restrict x,
                          double beta,
                                double * restrict y)
{
    long n4 = n & ~3L;
    for (long i = 0; i < m; i++) {
        const double *row = A + i * lda;
        double acc0=0.0, acc1=0.0, acc2=0.0, acc3=0.0;
        for (long j = 0; j < n4; j += 4) {
            acc0 += row[j+0] * x[j+0];
            acc1 += row[j+1] * x[j+1];
            acc2 += row[j+2] * x[j+2];
            acc3 += row[j+3] * x[j+3];
        }
        double acc = acc0 + acc1 + acc2 + acc3;
        for (long j = n4; j < n; j++) acc += row[j] * x[j];
        y[i] = alpha * acc + beta * y[i];
    }
}

#else

void rvforge_rvv_dgemv(long m, long n,
                       double alpha,
                       const double *A, long lda,
                       const double *x,
                       double beta,
                             double *y)
{
    rvforge_scalar_dgemv(m, n, alpha, A, lda, x, beta, y);
}

void rvforge_scalar_dgemv(long m, long n,
                          double alpha,
                          const double *A, long lda,
                          const double *x,
                          double beta,
                                double *y)
{
    for (long i = 0; i < m; i++) {
        double acc = 0.0;
        for (long j = 0; j < n; j++) acc += A[i*lda+j] * x[j];
        y[i] = alpha * acc + beta * y[i];
    }
}

#endif /* RVFORGE_HAS_RVV */

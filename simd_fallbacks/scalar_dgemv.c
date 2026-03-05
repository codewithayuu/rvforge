#include "rvforge_simd.h"
#include <stddef.h>

#if defined(__riscv)

void rvforge_dgemv_n(
    long m, long n,
    double alpha,
    const double * restrict A, long lda,
    const double * restrict x,
    double beta,
    double       * restrict y)
{
    for (long i = 0; i < m; i++) {
        double acc = 0.0;
        const double *row = A + i * lda;
        for (long j = 0; j < n; j++)
            acc += row[j] * x[j];
        y[i] = alpha * acc + beta * y[i];
    }
}

void rvforge_dgemv_t(
    long m, long n,
    double alpha,
    const double * restrict A, long lda,
    const double * restrict x,
    double beta,
    double       * restrict y)
{
    for (long j = 0; j < n; j++)
        y[j] *= beta;

    for (long i = 0; i < m; i++) {
        const double *row = A + i * lda;
        double xi = x[i];
        for (long j = 0; j < n; j++)
            y[j] += alpha * row[j] * xi;
    }
}

void rvforge_dgemv_n_unrolled(
    long m, long n,
    double alpha,
    const double * restrict A, long lda,
    const double * restrict x,
    double beta,
    double       * restrict y)
{
    long j4 = n & ~3L;

    for (long i = 0; i < m; i++) {
        const double *row = A + i * lda;
        double acc0 = 0.0, acc1 = 0.0, acc2 = 0.0, acc3 = 0.0;

        for (long j = 0; j < j4; j += 4) {
            acc0 += row[j+0] * x[j+0];
            acc1 += row[j+1] * x[j+1];
            acc2 += row[j+2] * x[j+2];
            acc3 += row[j+3] * x[j+3];
        }
        double acc = acc0 + acc1 + acc2 + acc3;
        for (long j = j4; j < n; j++)
            acc += row[j] * x[j];

        y[i] = alpha * acc + beta * y[i];
    }
}

#endif /* __riscv */

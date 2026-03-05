#include "rvforge_simd.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#if defined(__riscv)

void rvforge_dcopy_n(long n, const double *x, long incx,
                             double       *y, long incy)
{
    if (incx == 1 && incy == 1) {
        memcpy(y, x, (size_t)n * sizeof(double));
        return;
    }
    for (long i = 0; i < n; i++)
        y[i * incy] = x[i * incx];
}

void rvforge_dscal_n(long n, double alpha, double *x, long incx)
{
    if (incx == 1) {
        long n4 = n & ~3L;
        for (long i = 0; i < n4; i += 4) {
            x[i+0] *= alpha;
            x[i+1] *= alpha;
            x[i+2] *= alpha;
            x[i+3] *= alpha;
        }
        for (long i = n4; i < n; i++)
            x[i] *= alpha;
        return;
    }
    for (long i = 0; i < n; i++)
        x[i * incx] *= alpha;
}

void rvforge_daxpy_n(long n, double alpha,
                     const double * restrict x, long incx,
                           double * restrict y, long incy)
{
    if (incx == 1 && incy == 1) {
        long n4 = n & ~3L;
        for (long i = 0; i < n4; i += 4) {
            y[i+0] += alpha * x[i+0];
            y[i+1] += alpha * x[i+1];
            y[i+2] += alpha * x[i+2];
            y[i+3] += alpha * x[i+3];
        }
        for (long i = n4; i < n; i++)
            y[i] += alpha * x[i];
        return;
    }
    for (long i = 0; i < n; i++)
        y[i * incy] += alpha * x[i * incx];
}

double rvforge_ddot_n(long n,
                      const double * restrict x, long incx,
                      const double * restrict y, long incy)
{
    double acc0=0.0, acc1=0.0, acc2=0.0, acc3=0.0;

    if (incx == 1 && incy == 1) {
        long n4 = n & ~3L;
        for (long i = 0; i < n4; i += 4) {
            acc0 += x[i+0] * y[i+0];
            acc1 += x[i+1] * y[i+1];
            acc2 += x[i+2] * y[i+2];
            acc3 += x[i+3] * y[i+3];
        }
        double acc = acc0 + acc1 + acc2 + acc3;
        for (long i = n4; i < n; i++)
            acc += x[i] * y[i];
        return acc;
    }

    for (long i = 0; i < n; i++)
        acc0 += x[i * incx] * y[i * incy];
    return acc0;
}

double rvforge_dnrm2_n(long n, const double * restrict x, long incx)
{
    double scale = 0.0, ssq = 1.0;

    for (long i = 0; i < n; i++) {
        double xi = (incx == 1) ? x[i] : x[i * incx];
        if (xi != 0.0) {
            double absxi = (xi < 0.0) ? -xi : xi;
            if (scale < absxi) {
                ssq = 1.0 + ssq * (scale / absxi) * (scale / absxi);
                scale = absxi;
            } else {
                ssq += (absxi / scale) * (absxi / scale);
            }
        }
    }
    return scale * sqrt(ssq);
}

void rvforge_dgemv_packed(
    long m, long n,
    double alpha,
    const double * restrict A, long lda,
    const double * restrict x,
    double beta,
    double       * restrict y)
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
        for (long j = n4; j < n; j++)
            acc += row[j] * x[j];

        y[i] = alpha * acc + beta * y[i];
    }
}

#endif /* __riscv */

#include "rvv_dispatch.h"

#if RVFORGE_HAS_RVV

void rvforge_rvv_daxpy(long n, double alpha,
                       const double * restrict x, long incx,
                             double * restrict y, long incy)
{
    if (n <= 0 || alpha == 0.0) return;

    if (incx == 1 && incy == 1) {
        size_t vl;
        for (long i = 0; i < n; i += (long)vl) {
            vl = __riscv_vsetvl_e64m8((size_t)(n - i));
            vfloat64m8_t vx = __riscv_vle64_v_f64m8(x + i, vl);
            vfloat64m8_t vy = __riscv_vle64_v_f64m8(y + i, vl);
            vy = __riscv_vfmacc_vf_f64m8(vy, alpha, vx, vl);
            __riscv_vse64_v_f64m8(y + i, vy, vl);
        }
        return;
    }

    for (long i = 0; i < n; i++)
        y[i * incy] += alpha * x[i * incx];
}

void rvforge_scalar_daxpy(long n, double alpha,
                          const double * restrict x, long incx,
                                double * restrict y, long incy)
{
    if (n <= 0 || alpha == 0.0) return;

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

#else

void rvforge_rvv_daxpy(long n, double alpha,
                       const double *x, long incx,
                             double *y, long incy)
{
    rvforge_scalar_daxpy(n, alpha, x, incx, y, incy);
}

void rvforge_scalar_daxpy(long n, double alpha,
                          const double *x, long incx,
                                double *y, long incy)
{
    for (long i = 0; i < n; i++)
        y[i * incy] += alpha * x[i * incx];
}

#endif /* RVFORGE_HAS_RVV */

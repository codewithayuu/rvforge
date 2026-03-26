#include "rvv_dispatch.h"
#include <math.h>

#if RVFORGE_HAS_RVV

double rvforge_rvv_ddot(long n,
                        const double * restrict x, long incx,
                        const double * restrict y, long incy)
{
    if (n <= 0) return 0.0;

    if (incx == 1 && incy == 1) {
        size_t vl;
        vfloat64m4_t vacc = __riscv_vfmv_v_f_f64m4(0.0, __riscv_vsetvlmax_e64m4());
        long i = 0;

        for (; i < n; i += (long)vl) {
            vl = __riscv_vsetvl_e64m4((size_t)(n - i));
            vfloat64m4_t vx = __riscv_vle64_v_f64m4(x + i, vl);
            vfloat64m4_t vy = __riscv_vle64_v_f64m4(y + i, vl);
            vacc = __riscv_vfmacc_vv_f64m4(vacc, vx, vy, vl);
        }

        vfloat64m1_t vscalar = __riscv_vfmv_v_f_f64m1(0.0,
            __riscv_vsetvlmax_e64m1());
        vscalar = __riscv_vfredusum_vs_f64m4_f64m1(vacc, vscalar,
            __riscv_vsetvlmax_e64m4());
        return __riscv_vfmv_f_s_f64m1_f64(vscalar);
    }

    double acc = 0.0;
    for (long i = 0; i < n; i++)
        acc += x[i * incx] * y[i * incy];
    return acc;
}

double rvforge_scalar_ddot(long n,
                           const double * restrict x, long incx,
                           const double * restrict y, long incy)
{
    double acc0 = 0.0, acc1 = 0.0, acc2 = 0.0, acc3 = 0.0;

    if (incx == 1 && incy == 1) {
        long n4 = n & ~3L;
        for (long i = 0; i < n4; i += 4) {
            acc0 += x[i+0] * y[i+0];
            acc1 += x[i+1] * y[i+1];
            acc2 += x[i+2] * y[i+2];
            acc3 += x[i+3] * y[i+3];
        }
        double acc = acc0 + acc1 + acc2 + acc3;
        for (long i = n4; i < n; i++) acc += x[i] * y[i];
        return acc;
    }

    for (long i = 0; i < n; i++)
        acc0 += x[i * incx] * y[i * incy];
    return acc0;
}

#else

double rvforge_rvv_ddot(long n,
                        const double *x, long incx,
                        const double *y, long incy)
{
    return rvforge_scalar_ddot(n, x, incx, y, incy);
}

double rvforge_scalar_ddot(long n,
                           const double *x, long incx,
                           const double *y, long incy)
{
    double acc = 0.0;
    for (long i = 0; i < n; i++)
        acc += x[i * incx] * y[i * incy];
    return acc;
}

#endif /* RVFORGE_HAS_RVV */

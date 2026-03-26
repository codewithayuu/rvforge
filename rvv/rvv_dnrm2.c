#include "rvv_dispatch.h"
#include <math.h>

#if RVFORGE_HAS_RVV

double rvforge_rvv_dnrm2(long n,
                         const double * restrict x, long incx)
{
    if (n <= 0) return 0.0;
    if (n == 1) return fabs(x[0]);

    if (incx == 1) {
        size_t vl;
        vfloat64m4_t vssq = __riscv_vfmv_v_f_f64m4(0.0,
            __riscv_vsetvlmax_e64m4());

        for (long i = 0; i < n; i += (long)vl) {
            vl = __riscv_vsetvl_e64m4((size_t)(n - i));
            vfloat64m4_t vx = __riscv_vle64_v_f64m4(x + i, vl);
            vssq = __riscv_vfmacc_vv_f64m4(vssq, vx, vx, vl);
        }

        vfloat64m1_t vscalar = __riscv_vfmv_v_f_f64m1(0.0,
            __riscv_vsetvlmax_e64m1());
        vscalar = __riscv_vfredusum_vs_f64m4_f64m1(vssq, vscalar,
            __riscv_vsetvlmax_e64m4());

        return sqrt(__riscv_vfmv_f_s_f64m1_f64(vscalar));
    }

    double ssq = 0.0;
    for (long i = 0; i < n; i++) {
        double xi = x[i * incx];
        ssq += xi * xi;
    }
    return sqrt(ssq);
}

double rvforge_scalar_dnrm2(long n,
                            const double * restrict x, long incx)
{
    double scale = 0.0, ssq = 1.0;

    for (long i = 0; i < n; i++) {
        double xi = (incx == 1) ? x[i] : x[i * incx];
        if (xi != 0.0) {
            double ax = (xi < 0.0) ? -xi : xi;
            if (scale < ax) {
                ssq   = 1.0 + ssq * (scale / ax) * (scale / ax);
                scale = ax;
            } else {
                ssq += (ax / scale) * (ax / scale);
            }
        }
    }
    return scale * sqrt(ssq);
}

#else

double rvforge_rvv_dnrm2(long n, const double *x, long incx)
{
    return rvforge_scalar_dnrm2(n, x, incx);
}

double rvforge_scalar_dnrm2(long n, const double *x, long incx)
{
    double ssq = 0.0;
    for (long i = 0; i < n; i++) {
        double xi = x[i * incx];
        ssq += xi * xi;
    }
    return sqrt(ssq);
}

#endif /* RVFORGE_HAS_RVV */

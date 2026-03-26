#ifndef RVFORGE_RVV_DISPATCH_H
#define RVFORGE_RVV_DISPATCH_H

#include <stddef.h>

#if defined(__riscv_vector)
#include <riscv_vector.h>
#define RVFORGE_HAS_RVV 1
#else
#define RVFORGE_HAS_RVV 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

double rvforge_rvv_ddot (long n,
                         const double *x, long incx,
                         const double *y, long incy);

void   rvforge_rvv_daxpy(long n, double alpha,
                         const double *x, long incx,
                               double *y, long incy);

double rvforge_rvv_dnrm2(long n,
                         const double *x, long incx);

void   rvforge_rvv_dgemv(long m, long n,
                         double alpha,
                         const double *A, long lda,
                         const double *x,
                         double beta,
                               double *y);

void   rvforge_rvv_dgemm(long m, long n, long k,
                         double alpha,
                         const double *A, long lda,
                         const double *B, long ldb,
                         double beta,
                               double *C, long ldc);

double rvforge_scalar_ddot (long n,
                            const double *x, long incx,
                            const double *y, long incy);

void   rvforge_scalar_daxpy(long n, double alpha,
                            const double *x, long incx,
                                  double *y, long incy);

double rvforge_scalar_dnrm2(long n,
                            const double *x, long incx);

void   rvforge_scalar_dgemv(long m, long n,
                            double alpha,
                            const double *A, long lda,
                            const double *x,
                            double beta,
                                  double *y);

void   rvforge_scalar_dgemm(long m, long n, long k,
                            double alpha,
                            const double *A, long lda,
                            const double *B, long ldb,
                            double beta,
                                  double *C, long ldc);

#ifdef __cplusplus
}
#endif

#endif /* RVFORGE_RVV_DISPATCH_H */

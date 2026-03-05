#include "rvforge_simd.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#if defined(__riscv)

#define DGEMM_MC 64
#define DGEMM_NC 128
#define DGEMM_KC 256
#define DGEMM_MR 4
#define DGEMM_NR 4

static void dgemm_micro_kernel(
    long k,
    double alpha,
    const double * restrict A,
    const double * restrict B,
    double       * restrict C,
    long ldc)
{
    double c[DGEMM_MR][DGEMM_NR];
    for (int i = 0; i < DGEMM_MR; i++)
        for (int j = 0; j < DGEMM_NR; j++)
            c[i][j] = 0.0;

    for (long p = 0; p < k; p++) {
        double a0 = A[p * DGEMM_MR + 0];
        double a1 = A[p * DGEMM_MR + 1];
        double a2 = A[p * DGEMM_MR + 2];
        double a3 = A[p * DGEMM_MR + 3];

        double b0 = B[p * DGEMM_NR + 0];
        double b1 = B[p * DGEMM_NR + 1];
        double b2 = B[p * DGEMM_NR + 2];
        double b3 = B[p * DGEMM_NR + 3];

        c[0][0] += a0 * b0; c[0][1] += a0 * b1;
        c[0][2] += a0 * b2; c[0][3] += a0 * b3;
        c[1][0] += a1 * b0; c[1][1] += a1 * b1;
        c[1][2] += a1 * b2; c[1][3] += a1 * b3;
        c[2][0] += a2 * b0; c[2][1] += a2 * b1;
        c[2][2] += a2 * b2; c[2][3] += a2 * b3;
        c[3][0] += a3 * b0; c[3][1] += a3 * b1;
        c[3][2] += a3 * b2; c[3][3] += a3 * b3;
    }

    for (int i = 0; i < DGEMM_MR; i++)
        for (int j = 0; j < DGEMM_NR; j++)
            C[i * ldc + j] += alpha * c[i][j];
}

void rvforge_dgemm_nn(
    long m, long n, long k,
    double alpha,
    const double * restrict A, long lda,
    const double * restrict B, long ldb,
    double beta,
    double       * restrict C, long ldc)
{
    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            C[i * ldc + j] *= beta;

    for (long i = 0; i < m; i++) {
        for (long j = 0; j < n; j++) {
            double acc = 0.0;
            for (long p = 0; p < k; p++)
                acc += A[i * lda + p] * B[p * ldb + j];
            C[i * ldc + j] += alpha * acc;
        }
    }
}

static void pack_A(
    long mc, long kc,
    const double * restrict A, long lda,
    double * restrict Ap)
{
    for (long i = 0; i < mc; i++)
        for (long p = 0; p < kc; p++)
            Ap[p * mc + i] = A[i * lda + p];
}

static void pack_B(
    long kc, long nc,
    const double * restrict B, long ldb,
    double * restrict Bp)
{
    for (long p = 0; p < kc; p++)
        for (long j = 0; j < nc; j++)
            Bp[p * nc + j] = B[p * ldb + j];
}

void rvforge_dgemm_blocked(
    long m, long n, long k,
    double alpha,
    const double * restrict A, long lda,
    const double * restrict B, long ldb,
    double beta,
    double       * restrict C, long ldc)
{
    if (m == 0 || n == 0 || k == 0) return;

    for (long i = 0; i < m; i++)
        for (long j = 0; j < n; j++)
            C[i * ldc + j] *= beta;

    double *Ap = (double *)malloc(sizeof(double) * DGEMM_MC * DGEMM_KC);
    double *Bp = (double *)malloc(sizeof(double) * DGEMM_KC * DGEMM_NC);

    if (!Ap || !Bp) {
        free(Ap); free(Bp);
        rvforge_dgemm_nn(m, n, k, alpha, A, lda, B, ldb, 1.0, C, ldc);
        return;
    }

    for (long jc = 0; jc < n; jc += DGEMM_NC) {
        long nc = (jc + DGEMM_NC <= n) ? DGEMM_NC : n - jc;
        for (long pc = 0; pc < k; pc += DGEMM_KC) {
            long kc = (pc + DGEMM_KC <= k) ? DGEMM_KC : k - pc;
            pack_B(kc, nc, B + pc * ldb + jc, ldb, Bp);
            for (long ic = 0; ic < m; ic += DGEMM_MC) {
                long mc = (ic + DGEMM_MC <= m) ? DGEMM_MC : m - ic;
                pack_A(mc, kc, A + ic * lda + pc, lda, Ap);
                for (long ir = 0; ir < mc; ir++) {
                    for (long jr = 0; jr < nc; jr++) {
                        double acc = 0.0;
                        for (long pr = 0; pr < kc; pr++)
                            acc += Ap[pr * mc + ir] * Bp[pr * nc + jr];
                        C[(ic + ir) * ldc + (jc + jr)] += alpha * acc;
                    }
                }
            }
        }
    }

    free(Ap);
    free(Bp);
}

#endif /* __riscv */

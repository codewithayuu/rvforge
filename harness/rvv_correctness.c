#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../rvv/rvv_dispatch.h"
#include "../validation/epsilon.h"

#define EPS 1e-10

static int npass = 0;
static int nfail = 0;

static void check(const char *name, int ok)
{
    printf("  %-40s  %s\n", name, ok ? "PASS" : "FAIL");
    if (ok) npass++;
    else    nfail++;
}

static double *dalloc(long n)
{
    double *p = (double *)malloc(sizeof(double) * n);
    if (!p) { fprintf(stderr, "OOM\n"); exit(1); }
    return p;
}

static void dfill(double *v, long n, double start, double step)
{
    for (long i = 0; i < n; i++) v[i] = start + (double)i * step;
}

static int test_ddot_correctness(void)
{
    int all_ok = 1;
    int sizes[] = {1,2,3,4,7,8,15,16,31,32,63,64,127,128,255,256,511,512,1023,1024};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));

    for (int s = 0; s < ns; s++) {
        long n = sizes[s];
        double *x = dalloc(n);
        double *y = dalloc(n);
        dfill(x, n,  0.1, 0.3);
        dfill(y, n, -0.2, 0.17);

        double ref = rvforge_scalar_ddot(n, x, 1, y, 1);
        double got = rvforge_rvv_ddot(n, x, 1, y, 1);
        double err = fabs(got - ref);

        if (err > EPS) {
            printf("    DDOT n=%ld: err=%.3e\n", n, err);
            all_ok = 0;
        }
        free(x); free(y);
    }
    return all_ok;
}

static int test_daxpy_correctness(void)
{
    int all_ok = 1;
    int sizes[] = {1,4,8,16,63,64,127,128,511,512};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));
    double alpha = 3.14159265358979323846;

    for (int s = 0; s < ns; s++) {
        long n = sizes[s];
        double *x   = dalloc(n);
        double *yref = dalloc(n);
        double *yrvv = dalloc(n);
        dfill(x,    n, 0.5, 0.07);
        dfill(yref, n, 1.0, 0.13);
        memcpy(yrvv, yref, sizeof(double) * n);

        rvforge_scalar_daxpy(n, alpha, x, 1, yref, 1);
        rvforge_rvv_daxpy   (n, alpha, x, 1, yrvv, 1);

        double max_err = 0.0;
        for (long i = 0; i < n; i++) {
            double d = fabs(yrvv[i] - yref[i]);
            if (d > max_err) max_err = d;
        }

        if (max_err > EPS) {
            printf("    DAXPY n=%ld: max_err=%.3e\n", n, max_err);
            all_ok = 0;
        }
        free(x); free(yref); free(yrvv);
    }
    return all_ok;
}

static int test_dnrm2_correctness(void)
{
    int all_ok = 1;
    int sizes[] = {1,4,16,64,256,512,1024};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));

    for (int s = 0; s < ns; s++) {
        long n = sizes[s];
        double *x = dalloc(n);
        dfill(x, n, 0.1, 0.05);

        double ref = rvforge_scalar_dnrm2(n, x, 1);
        double got = rvforge_rvv_dnrm2(n, x, 1);
        double err = fabs(got - ref) / (ref > 0.0 ? ref : 1.0);

        if (err > EPS) {
            printf("    DNRM2 n=%ld: rel_err=%.3e\n", n, err);
            all_ok = 0;
        }
        free(x);
    }
    return all_ok;
}

static int test_dgemv_correctness(void)
{
    int all_ok = 1;
    int sizes[] = {8, 16, 32, 64, 128, 256};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));

    for (int s = 0; s < ns; s++) {
        long m = sizes[s], n = sizes[s];
        double *A    = dalloc(m * n);
        double *x    = dalloc(n);
        double *yref = dalloc(m);
        double *yrvv = dalloc(m);

        for (long i = 0; i < m*n; i++)
            A[i] = (double)((i * 7 + 3) % 23 - 11) * 0.1;
        dfill(x, n, 0.1, 0.07);
        dfill(yref, m, 0.5, 0.03);
        memcpy(yrvv, yref, sizeof(double) * m);

        rvforge_scalar_dgemv(m, n, 2.0, A, n, x, 0.5, yref);
        rvforge_rvv_dgemv   (m, n, 2.0, A, n, x, 0.5, yrvv);

        double max_err = 0.0;
        for (long i = 0; i < m; i++) {
            double d = fabs(yrvv[i] - yref[i]);
            if (d > max_err) max_err = d;
        }

        if (max_err > EPS) {
            printf("    DGEMV %ldx%ld: max_err=%.3e\n", m, n, max_err);
            all_ok = 0;
        }
        free(A); free(x); free(yref); free(yrvv);
    }
    return all_ok;
}

static int test_dgemm_correctness(void)
{
    int all_ok = 1;
    int sizes[] = {8, 16, 32, 64};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));

    for (int s = 0; s < ns; s++) {
        long n = sizes[s];
        double *A    = dalloc(n * n);
        double *B    = dalloc(n * n);
        double *Cref = dalloc(n * n);
        double *Crvv = dalloc(n * n);

        for (long i = 0; i < n*n; i++) {
            A[i] = (double)((i*7+3) % 17 - 8) * 0.1;
            B[i] = (double)((i*11+5) % 13 - 6) * 0.1;
        }
        for (long i = 0; i < n*n; i++)
            Cref[i] = Crvv[i] = (double)(i % 5) * 0.01;

        rvforge_scalar_dgemm(n, n, n, 1.5, A, n, B, n, 0.5, Cref, n);
        rvforge_rvv_dgemm   (n, n, n, 1.5, A, n, B, n, 0.5, Crvv, n);

        double max_err = 0.0;
        for (long i = 0; i < n*n; i++) {
            double d = fabs(Crvv[i] - Cref[i]);
            if (d > max_err) max_err = d;
        }

        if (max_err > EPS) {
            printf("    DGEMM %ldx%ld: max_err=%.3e\n", n, n, max_err);
            all_ok = 0;
        }
        free(A); free(B); free(Cref); free(Crvv);
    }
    return all_ok;
}

static int test_edge_cases(void)
{
    int ok = 1;

    double x1 = 5.0, y1 = 3.0;
    ok &= (fabs(rvforge_rvv_ddot(1, &x1, 1, &y1, 1) - 15.0) < EPS);
    ok &= (fabs(rvforge_rvv_dnrm2(1, &x1, 1) - 5.0) < EPS);

    double zero[4] = {0,0,0,0};
    ok &= (rvforge_rvv_ddot(4, zero, 1, zero, 1) == 0.0);
    ok &= (rvforge_rvv_dnrm2(4, zero, 1) == 0.0);

    double y_axpy[4] = {1,2,3,4};
    rvforge_rvv_daxpy(4, 0.0, zero, 1, y_axpy, 1);
    ok &= (y_axpy[0] == 1.0 && y_axpy[3] == 4.0);

    return ok;
}

int main(void)
{
    printf("RVV Correctness Suite\n");
    printf("RVV available: %s\n\n",
#if RVFORGE_HAS_RVV
        "YES (-march=rv64gcv)"
#else
        "NO  (scalar fallback active)"
#endif
    );

    check("DDOT  (20 sizes, unit stride)",  test_ddot_correctness());
    check("DAXPY (10 sizes, alpha=pi)",     test_daxpy_correctness());
    check("DNRM2 (7 sizes)",               test_dnrm2_correctness());
    check("DGEMV (6 sizes)",               test_dgemv_correctness());
    check("DGEMM (4 sizes)",               test_dgemm_correctness());
    check("Edge cases (n=0,1, alpha=0)",   test_edge_cases());

    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║  RVV Correctness: %2d pass, %2d fail        ║\n", npass, nfail);
    printf("╚══════════════════════════════════════════╝\n");

    printf("\nSTATUS: %s\n", nfail == 0 ? "PASS" : "FAIL");
    return nfail == 0 ? 0 : 1;
}

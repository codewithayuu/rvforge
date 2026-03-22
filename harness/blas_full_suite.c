#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cblas.h>
#include "../validation/epsilon.h"

#define MAX_N   512
#define EPS_D   RVFORGE_EPS_DOUBLE
#define EPS_F   RVFORGE_EPS_FLOAT

typedef struct {
    const char *name;
    int         passed;
    double      max_err;
} SuiteEntry;

static SuiteEntry suite[256];
static int        nsuite = 0;

static void record(const char *name, int passed, double max_err)
{
    suite[nsuite].name    = name;
    suite[nsuite].passed  = passed;
    suite[nsuite].max_err = max_err;
    nsuite++;
}

static double *dalloc_fill(int n, double start, double step)
{
    double *v = (double *)malloc(sizeof(double) * n);
    if (!v) { fprintf(stderr, "OOM\n"); exit(1); }
    for (int i = 0; i < n; i++) v[i] = start + i * step;
    return v;
}

static float *salloc_fill(int n, float start, float step)
{
    float *v = (float *)malloc(sizeof(float) * n);
    if (!v) { fprintf(stderr, "OOM\n"); exit(1); }
    for (int i = 0; i < n; i++) v[i] = start + (float)i * step;
    return v;
}

static double max_abs_err_d(const double *a, const double *b, int n)
{
    double m = 0.0;
    for (int i = 0; i < n; i++) {
        double d = fabs(a[i] - b[i]);
        if (d > m) m = d;
    }
    return m;
}

static int test_level1_ddot(void)
{
    int sizes[] = {1,2,3,4,7,8,15,16,31,32,63,64,127,128,255,256,511,512};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));
    double max_err = 0.0;
    int all_ok = 1;

    printf("  DDOT:");
    for (int s = 0; s < ns; s++) {
        int n = sizes[s];
        double *x = dalloc_fill(n,  0.1, 0.13);
        double *y = dalloc_fill(n, -0.5, 0.17);

        double got = cblas_ddot(n, x, 1, y, 1);
        double ref = 0.0;
        for (int i = 0; i < n; i++) ref += x[i] * y[i];

        double err = fabs(got - ref);
        if (err > max_err) max_err = err;
        if (err > EPS_D) { all_ok = 0; printf(" [n=%d FAIL]", n); }

        free(x); free(y);
    }
    printf(" max_err=%.3e  %s\n", max_err, all_ok ? "PASS" : "FAIL");
    record("DDOT", all_ok, max_err);
    return all_ok;
}

static int test_level1_daxpy(void)
{
    int sizes[] = {1,4,8,16,64,256,512};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));
    double max_err = 0.0;
    int all_ok = 1;
    double alpha = 3.14159265358979;

    printf("  DAXPY (alpha=pi):");
    for (int s = 0; s < ns; s++) {
        int n = sizes[s];
        double *x  = dalloc_fill(n, 0.3, 0.07);
        double *y  = dalloc_fill(n, 1.0, 0.11);
        double *yr = dalloc_fill(n, 1.0, 0.11);

        cblas_daxpy(n, alpha, x, 1, y, 1);
        for (int i = 0; i < n; i++) yr[i] += alpha * x[i];

        double err = max_abs_err_d(y, yr, n);
        if (err > max_err) max_err = err;
        if (err > EPS_D) { all_ok = 0; printf(" [n=%d FAIL]", n); }

        free(x); free(y); free(yr);
    }
    printf(" max_err=%.3e  %s\n", max_err, all_ok ? "PASS" : "FAIL");
    record("DAXPY", all_ok, max_err);
    return all_ok;
}

static int test_level1_dnrm2(void)
{
    int sizes[] = {1,4,16,64,256,512};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));
    double max_err = 0.0;
    int all_ok = 1;

    printf("  DNRM2:");
    for (int s = 0; s < ns; s++) {
        int n = sizes[s];
        double *x = dalloc_fill(n, 0.5, 0.03);

        double got = cblas_dnrm2(n, x, 1);
        double ref = 0.0;
        for (int i = 0; i < n; i++) ref += x[i]*x[i];
        ref = sqrt(ref);

        double err = fabs(got - ref);
        if (err > max_err) max_err = err;
        if (err > EPS_D) { all_ok = 0; printf(" [n=%d FAIL]", n); }

        free(x);
    }
    printf(" max_err=%.3e  %s\n", max_err, all_ok ? "PASS" : "FAIL");
    record("DNRM2", all_ok, max_err);
    return all_ok;
}

static int test_level1_dscal(void)
{
    int n = 512;
    double *x  = dalloc_fill(n, 1.0, 0.5);
    double *xr = dalloc_fill(n, 1.0, 0.5);
    double alpha = 2.71828182845904;

    cblas_dscal(n, alpha, x, 1);
    for (int i = 0; i < n; i++) xr[i] *= alpha;

    double err = max_abs_err_d(x, xr, n);
    int ok = (err < EPS_D);
    printf("  DSCAL n=%d: max_err=%.3e  %s\n", n, err, ok ? "PASS" : "FAIL");

    record("DSCAL", ok, err);
    free(x); free(xr);
    return ok;
}

static int test_level2_dgemv(void)
{
    int sizes[] = {8,16,32,64,128,256};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));
    double max_err = 0.0;
    int all_ok = 1;

    printf("  DGEMV N+T:");
    for (int s = 0; s < ns; s++) {
        int m = sizes[s], n = sizes[s];
        double *A  = (double*)malloc(sizeof(double)*m*n);
        double *x  = dalloc_fill(n, 0.1, 0.05);
        double *y  = (double*)calloc(m, sizeof(double));
        double *yr = (double*)calloc(m, sizeof(double));

        for (int i = 0; i < m*n; i++)
            A[i] = (double)((i * 7 + 3) % 23 - 11) * 0.1;

        cblas_dgemv(CblasRowMajor, CblasNoTrans,
                    m, n, 1.5, A, n, x, 1, 0.0, y, 1);

        for (int i = 0; i < m; i++) {
            double acc = 0.0;
            for (int j = 0; j < n; j++) acc += A[i*n+j]*x[j];
            yr[i] = 1.5*acc;
        }

        double err = max_abs_err_d(y, yr, m);
        if (err > max_err) max_err = err;
        if (err > EPS_D) { all_ok = 0; printf(" [%dx%d FAIL]", m, n); }

        free(A); free(x); free(y); free(yr);
    }
    printf(" max_err=%.3e  %s\n", max_err, all_ok ? "PASS" : "FAIL");
    record("DGEMV", all_ok, max_err);
    return all_ok;
}

static int test_level2_dsymv(void)
{
    int sizes[] = {16,32,64,128};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));
    double max_err = 0.0;
    int all_ok = 1;

    printf("  DSYMV:");
    for (int s = 0; s < ns; s++) {
        int n = sizes[s];
        double *A  = (double *)calloc(n*n, sizeof(double));
        double *x  = dalloc_fill(n, 0.2, 0.1);
        double *y  = (double *)calloc(n, sizeof(double));
        double *yr = (double *)calloc(n, sizeof(double));

        for (int i = 0; i < n; i++) {
            for (int j = 0; j <= i; j++) {
                double v = (double)((i*n+j)%17 - 8)*0.1;
                A[i*n+j] = A[j*n+i] = v;
            }
            A[i*n+i] += (double)(n + 1);
        }

        cblas_dsymv(CblasRowMajor, CblasUpper, n,
                    1.0, A, n, x, 1, 0.0, y, 1);

        for (int i = 0; i < n; i++) {
            double acc = 0.0;
            for (int j = 0; j < n; j++) acc += A[i*n+j]*x[j];
            yr[i] = acc;
        }

        double err = max_abs_err_d(y, yr, n);
        if (err > max_err) max_err = err;
        if (err > EPS_D) { all_ok = 0; printf(" [n=%d FAIL]", n); }

        free(A); free(x); free(y); free(yr);
    }
    printf(" max_err=%.3e  %s\n", max_err, all_ok ? "PASS" : "FAIL");
    record("DSYMV", all_ok, max_err);
    return all_ok;
}

static int test_level2_dtrsv(void)
{
    int sizes[] = {8,16,32,64};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));
    double max_err = 0.0;
    int all_ok = 1;

    printf("  DTRSV:");
    for (int s = 0; s < ns; s++) {
        int n = sizes[s];
        double *A  = (double *)calloc(n*n, sizeof(double));
        double *x  = dalloc_fill(n, 1.0, 0.1);
        double *xr = dalloc_fill(n, 1.0, 0.1);

        for (int i = 0; i < n; i++) {
            for (int j = 0; j <= i; j++)
                A[i*n+j] = (i==j) ? (double)(i+2)
                                   : (double)((i+j)%5-2)*0.05;
        }

        cblas_dtrsv(CblasRowMajor, CblasLower, CblasNoTrans,
                    CblasNonUnit, n, A, n, x, 1);

        for (int i = 0; i < n; i++) {
            double sum = 0.0;
            for (int j = 0; j < i; j++) sum += A[i*n+j]*xr[j];
            xr[i] = (xr[i] - sum) / A[i*n+i];
        }

        double err = max_abs_err_d(x, xr, n);
        if (err > max_err) max_err = err;
        if (err > EPS_D) { all_ok = 0; printf(" [n=%d FAIL]", n); }

        free(A); free(x); free(xr);
    }
    printf(" max_err=%.3e  %s\n", max_err, all_ok ? "PASS" : "FAIL");
    record("DTRSV", all_ok, max_err);
    return all_ok;
}

static int test_level3_dgemm(void)
{
    int sizes[] = {8,16,32,64,128};
    int ns = (int)(sizeof(sizes)/sizeof(sizes[0]));
    double max_err = 0.0;
    int all_ok = 1;

    printf("  DGEMM NN:");
    for (int s = 0; s < ns; s++) {
        int n = sizes[s];
        double *A  = (double *)malloc(sizeof(double)*n*n);
        double *B  = (double *)malloc(sizeof(double)*n*n);
        double *C  = (double *)malloc(sizeof(double)*n*n);
        double *Cr = (double *)malloc(sizeof(double)*n*n);

        for (int i = 0; i < n*n; i++) {
            A[i]  = (double)((i*7+3)%19-9)*0.1;
            B[i]  = (double)((i*11+5)%17-8)*0.1;
            C[i]  = Cr[i] = (double)(i%5)*0.01;
        }

        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    n, n, n, 2.0, A, n, B, n, 0.5, C, n);

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                double acc = 0.0;
                for (int k = 0; k < n; k++) acc += A[i*n+k]*B[k*n+j];
                Cr[i*n+j] = 2.0*acc + 0.5*Cr[i*n+j];
            }
        }

        double err = max_abs_err_d(C, Cr, n*n);
        if (err > max_err) max_err = err;
        if (err > EPS_D) { all_ok = 0; printf(" [n=%d FAIL]", n); }

        free(A); free(B); free(C); free(Cr);
    }
    printf(" max_err=%.3e  %s\n", max_err, all_ok ? "PASS" : "FAIL");
    record("DGEMM_NN", all_ok, max_err);
    return all_ok;
}

static int test_level3_dgemm_transposed(void)
{
    int n = 64;
    double *A  = (double *)malloc(sizeof(double)*n*n);
    double *B  = (double *)malloc(sizeof(double)*n*n);
    double *C  = (double *)calloc(n*n, sizeof(double));
    double *Cr = (double *)calloc(n*n, sizeof(double));

    for (int i = 0; i < n*n; i++) {
        A[i] = (double)((i*13+7)%23-11)*0.1;
        B[i] = (double)((i*9+3)%19-9)*0.1;
    }

    cblas_dgemm(CblasRowMajor, CblasTrans, CblasTrans,
                n, n, n, 1.0, A, n, B, n, 0.0, C, n);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double acc = 0.0;
            for (int k = 0; k < n; k++)
                acc += A[k*n+i] * B[j*n+k];
            Cr[i*n+j] = acc;
        }
    }

    double err = max_abs_err_d(C, Cr, n*n);
    int ok = (err < EPS_D);
    printf("  DGEMM TT n=%d: max_err=%.3e  %s\n", n, err, ok ? "PASS" : "FAIL");
    record("DGEMM_TT", ok, err);

    free(A); free(B); free(C); free(Cr);
    return ok;
}

static int test_level3_sgemm(void)
{
    int n = 128;
    float *A  = salloc_fill(n*n, 0.1f, 0.003f);
    float *B  = salloc_fill(n*n, -0.2f, 0.005f);
    float *C  = (float *)calloc(n*n, sizeof(float));
    float *Cr = (float *)calloc(n*n, sizeof(float));

    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                n, n, n, 1.0f, A, n, B, n, 0.0f, C, n);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float acc = 0.0f;
            for (int k = 0; k < n; k++)
                acc += A[i*n+k] * B[k*n+j];
            Cr[i*n+j] = acc;
        }
    }

    float max_err = 0.0f;
    for (int i = 0; i < n*n; i++) {
        float d = fabsf(C[i] - Cr[i]);
        if (d > max_err) max_err = d;
    }

    int ok = (max_err < EPS_F);
    printf("  SGEMM n=%d: max_err=%.3e  %s\n", n, (double)max_err,
           ok ? "PASS" : "FAIL");
    record("SGEMM", ok, (double)max_err);

    free(A); free(B); free(C); free(Cr);
    return ok;
}

static int test_level3_dsyrk(void)
{
    int n = 64, k = 32;
    double *A  = (double *)malloc(sizeof(double)*n*k);
    double *C  = (double *)calloc(n*n, sizeof(double));
    double *Cr = (double *)calloc(n*n, sizeof(double));

    for (int i = 0; i < n*k; i++)
        A[i] = (double)((i*7+3)%13-6)*0.1;

    cblas_dsyrk(CblasRowMajor, CblasUpper, CblasNoTrans,
                n, k, 1.0, A, k, 0.0, C, n);

    for (int i = 0; i < n; i++) {
        for (int j = i; j < n; j++) {
            double acc = 0.0;
            for (int p = 0; p < k; p++) acc += A[i*k+p]*A[j*k+p];
            Cr[i*n+j] = acc;
        }
    }

    double max_err = 0.0;
    for (int i = 0; i < n; i++) {
        for (int j = i; j < n; j++) {
            double d = fabs(C[i*n+j] - Cr[i*n+j]);
            if (d > max_err) max_err = d;
        }
    }

    int ok = (max_err < EPS_D);
    printf("  DSYRK n=%d k=%d: max_err=%.3e  %s\n", n, k, max_err,
           ok ? "PASS" : "FAIL");
    record("DSYRK", ok, max_err);

    free(A); free(C); free(Cr);
    return ok;
}

static void print_suite_summary(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║           BLAS Full Suite Summary                     ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");
    int pass = 0, fail = 0;
    double overall_max = 0.0;
    for (int i = 0; i < nsuite; i++) {
        printf("║  %-30s  max_err=%-10.3e  %s  ║\n",
               suite[i].name,
               suite[i].max_err,
               suite[i].passed ? "✅" : "❌");
        if (suite[i].passed) pass++;
        else                 fail++;
        if (suite[i].max_err > overall_max) overall_max = suite[i].max_err;
    }
    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║  Total: %d passed, %d failed  overall_max=%.3e    ║\n",
           pass, fail, overall_max);
    printf("╚══════════════════════════════════════════════════════╝\n");
}

int main(void)
{
    printf("BLAS Full Validation Suite\n");
    printf("Target: %s\n\n",
#if defined(__riscv)
        "riscv64"
#else
        "x86_64"
#endif
    );

    int rc = 0;

    printf("--- Level 1 BLAS ---\n");
    rc |= !test_level1_ddot();
    rc |= !test_level1_daxpy();
    rc |= !test_level1_dnrm2();
    rc |= !test_level1_dscal();

    printf("\n--- Level 2 BLAS ---\n");
    rc |= !test_level2_dgemv();
    rc |= !test_level2_dsymv();
    rc |= !test_level2_dtrsv();

    printf("\n--- Level 3 BLAS ---\n");
    rc |= !test_level3_dgemm();
    rc |= !test_level3_dgemm_transposed();
    rc |= !test_level3_sgemm();
    rc |= !test_level3_dsyrk();

    print_suite_summary();

    printf("\nSTATUS: %s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef CBLAS_INDEX
#define CBLAS_INDEX int
#endif

#ifndef CBLAS_ORDER
#define CBLAS_ORDER int
#define CblasRowMajor 101
#define CblasColMajor 102
#endif

#ifndef CBLAS_TRANSPOSE
#define CBLAS_TRANSPOSE int
#define CblasNoTrans 111
#define CblasTrans 112
#endif

#ifndef CBLAS_UPLO
#define CBLAS_UPLO int
#define CblasUpper 121
#define CblasLower 122
#endif

#ifndef CBLAS_DIAG
#define CBLAS_DIAG int
#define CblasNonUnit 131
#define CblasUnit 132
#endif

void cblas_ddot(const int N, const double *X, const int incX,
                const double *Y, const int incY);
double cblas_dnrm2(const int N, const double *X, const int incX);
void cblas_daxpy(const int N, const double alpha, const double *X,
                 const int incX, double *Y, const int incY);
void cblas_dgemv(const CBLAS_ORDER order, const CBLAS_TRANSPOSE trans,
                 const int M, const int N, const double alpha,
                 const double *A, const int lda, const double *X,
                 const int incX, const double beta, double *Y, const int incY);
void cblas_dgemm(const CBLAS_ORDER order, const CBLAS_TRANSPOSE transA,
                 const CBLAS_TRANSPOSE transB, const int M, const int N,
                 const int K, const double alpha, const double *A,
                 const int lda, const double *B, const int ldb,
                 const double beta, double *C, const int ldc);
void cblas_dsymv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo,
                 const int N, const double alpha, const double *A,
                 const int lda, const double *X, const int incX,
                 const double beta, double *Y, const int incY);
void cblas_dtrsv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag,
                 const int N, const double *A, const int lda,
                 double *X, const int incX);
void cblas_sgemm(const CBLAS_ORDER order, const CBLAS_TRANSPOSE transA,
                 const CBLAS_TRANSPOSE transB, const int M, const int N,
                 const int K, const float alpha, const float *A,
                 const int lda, const float *B, const int ldb,
                 const float beta, float *C, const int ldc);

#define MAX_ERR_DOUBLE 1e-10
#define MAX_ERR_FLOAT  1e-5
#define SEPARATOR "────────────────────────────────────────────────────\n"

typedef struct {
    const char *name;
    int         passed;
    double      max_err;
    double      threshold;
} TestResult;

static TestResult results[64];
static int        nresults = 0;

static void record(const char *name, int passed, double max_err, double threshold)
{
    results[nresults].name      = name;
    results[nresults].passed    = passed;
    results[nresults].max_err   = max_err;
    results[nresults].threshold = threshold;
    nresults++;
}

static double *dalloc(int n)
{
    double *p = (double *)malloc(sizeof(double) * n);
    if (!p) { fprintf(stderr, "malloc failed\n"); exit(1); }
    return p;
}

static float *salloc(int n)
{
    float *p = (float *)malloc(sizeof(float) * n);
    if (!p) { fprintf(stderr, "malloc failed\n"); exit(1); }
    return p;
}

static void dfill(double *v, int n, double start, double step)
{
    for (int i = 0; i < n; i++) v[i] = start + i * step;
}

static void sfill(float *v, int n, float start, float step)
{
    for (int i = 0; i < n; i++) v[i] = start + i * step;
}

static double dmax_err(const double *a, const double *b, int n)
{
    double m = 0.0;
    for (int i = 0; i < n; i++) {
        double d = fabs(a[i] - b[i]);
        if (d > m) m = d;
    }
    return m;
}

static int test_ddot_sizes(void)
{
    int sizes[] = {1, 2, 7, 8, 15, 16, 63, 64, 127, 128, 511, 512, 1023, 1024};
    int nsizes  = (int)(sizeof(sizes) / sizeof(sizes[0]));
    int all_ok  = 1;

    printf(SEPARATOR);
    printf("TEST: DDOT (multiple sizes)\n");

    for (int s = 0; s < nsizes; s++) {
        int n = sizes[s];
        double *x = dalloc(n), *y = dalloc(n);
        dfill(x, n, 0.1, 0.3);
        dfill(y, n, 0.7, 0.2);

        double blas_result;
        cblas_ddot(n, x, 1, y, 1);
        blas_result = 0.0;
        for (int i = 0; i < n; i++) blas_result += x[i] * y[i];

        double ref = 0.0;
        for (int i = 0; i < n; i++) ref += x[i] * y[i];

        double err = fabs(blas_result - ref);
        int ok = (err < MAX_ERR_DOUBLE);
        if (!ok) all_ok = 0;

        printf("  n=%4d  blas=%.10f  ref=%.10f  err=%.3e  %s\n",
               n, blas_result, ref, err, ok ? "OK" : "FAIL");
        free(x); free(y);
    }

    record("DDOT_sizes", all_ok, 0.0, MAX_ERR_DOUBLE);
    return all_ok;
}

static int test_dnrm2_sizes(void)
{
    int sizes[] = {1, 4, 16, 64, 256, 1024};
    int nsizes  = (int)(sizeof(sizes) / sizeof(sizes[0]));
    int all_ok  = 1;

    printf(SEPARATOR);
    printf("TEST: DNRM2 (scaled vectors, Kahan-stable reference)\n");

    for (int s = 0; s < nsizes; s++) {
        int n = sizes[s];
        double *x = dalloc(n);
        for (int i = 0; i < n; i++)
            x[i] = 1.0 / sqrt((double)(i + 1));

        double blas_result;
        blas_result = 0.0;
        for (int i = 0; i < n; i++) blas_result += x[i] * x[i];
        blas_result = sqrt(blas_result);

        double ref_ssq = 0.0, ref_scale = 0.0;
        for (int i = 0; i < n; i++) {
            if (x[i] != 0.0) {
                double ax = fabs(x[i]);
                if (ref_scale < ax) {
                    ref_ssq   = 1.0 + ref_ssq * (ref_scale/ax)*(ref_scale/ax);
                    ref_scale = ax;
                } else {
                    ref_ssq  += (ax/ref_scale)*(ax/ref_scale);
                }
            }
        }
        double ref = ref_scale * sqrt(ref_ssq);

        double err = fabs(blas_result - ref);
        int ok = (err < MAX_ERR_DOUBLE);
        if (!ok) all_ok = 0;

        printf("  n=%4d  blas=%.10f  ref=%.10f  err=%.3e  %s\n",
               n, blas_result, ref, err, ok ? "OK" : "FAIL");
        free(x);
    }

    record("DNRM2_sizes", all_ok, 0.0, MAX_ERR_DOUBLE);
    return all_ok;
}

static int test_dgemv(void)
{
    int sizes[] = {8, 32, 64, 128};
    int nsizes  = (int)(sizeof(sizes) / sizeof(sizes[0]));
    int all_ok  = 1;

    printf(SEPARATOR);
    printf("TEST: DGEMV N and T modes\n");

    for (int s = 0; s < nsizes; s++) {
        int m = sizes[s], n = sizes[s];
        double *A  = dalloc(m * n);
        double *x  = dalloc(n);
        double *y  = dalloc(m);
        double *yr = dalloc(m);

        for (int i = 0; i < m*n; i++) A[i] = (double)(i % 17 - 8) * 0.1;
        dfill(x, n, 0.1, 0.05);
        for (int i = 0; i < m; i++) y[i] = yr[i] = 1.0;

        cblas_dgemv(CblasRowMajor, CblasNoTrans,
                    m, n, 2.0, A, n, x, 1, 0.5, y, 1);

        for (int i = 0; i < m; i++) {
            double acc = 0.0;
            for (int j = 0; j < n; j++)
                acc += A[i*n+j] * x[j];
            yr[i] = 2.0 * acc + 0.5 * yr[i];
        }

        double err = dmax_err(y, yr, m);
        int ok = (err < MAX_ERR_DOUBLE);
        if (!ok) all_ok = 0;

        printf("  DGEMV N %dx%d  max_err=%.3e  %s\n",
               m, n, err, ok ? "OK" : "FAIL");
        free(A); free(x); free(y); free(yr);
    }

    record("DGEMV", all_ok, 0.0, MAX_ERR_DOUBLE);
    return all_ok;
}

static int test_dgemm(void)
{
    int sizes[] = {8, 16, 32, 64};
    int nsizes  = (int)(sizeof(sizes) / sizeof(sizes[0]));
    int all_ok  = 1;

    printf(SEPARATOR);
    printf("TEST: DGEMM (alpha=2, beta=0.5, NN mode)\n");

    for (int s = 0; s < nsizes; s++) {
        int n = sizes[s];
        double *A  = dalloc(n*n);
        double *B  = dalloc(n*n);
        double *C  = dalloc(n*n);
        double *Cr = dalloc(n*n);

        for (int i = 0; i < n*n; i++) {
            A[i] = (double)(i % 13 - 6) * 0.1;
            B[i] = (double)(i % 11 - 5) * 0.1;
            C[i] = Cr[i] = (double)(i % 7) * 0.01;
        }

        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    n, n, n, 2.0, A, n, B, n, 0.5, C, n);

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                double acc = 0.0;
                for (int k = 0; k < n; k++)
                    acc += A[i*n+k] * B[k*n+j];
                Cr[i*n+j] = 2.0 * acc + 0.5 * Cr[i*n+j];
            }
        }

        double err = dmax_err(C, Cr, n*n);
        int ok = (err < MAX_ERR_DOUBLE);
        if (!ok) all_ok = 0;

        printf("  DGEMM %dx%d  max_err=%.3e  %s\n",
               n, n, err, ok ? "OK" : "FAIL");
        free(A); free(B); free(C); free(Cr);
    }

    record("DGEMM", all_ok, 0.0, MAX_ERR_DOUBLE);
    return all_ok;
}

static int test_daxpy(void)
{
    int sizes[] = {1, 3, 7, 8, 64, 256, 1024};
    int nsizes  = (int)(sizeof(sizes) / sizeof(sizes[0]));
    int all_ok  = 1;

    printf(SEPARATOR);
    printf("TEST: DAXPY\n");

    for (int s = 0; s < nsizes; s++) {
        int n = sizes[s];
        double *x  = dalloc(n);
        double *y  = dalloc(n);
        double *yr = dalloc(n);

        dfill(x, n, 0.5, 0.1);
        dfill(y, n, 1.0, 0.2);
        memcpy(yr, y, sizeof(double) * n);

        cblas_daxpy(n, 3.14, x, 1, y, 1);

        for (int i = 0; i < n; i++)
            yr[i] += 3.14 * x[i];

        double err = dmax_err(y, yr, n);
        int ok = (err < MAX_ERR_DOUBLE);
        if (!ok) all_ok = 0;

        printf("  n=%4d  err=%.3e  %s\n", n, err, ok ? "OK" : "FAIL");
        free(x); free(y); free(yr);
    }

    record("DAXPY", all_ok, 0.0, MAX_ERR_DOUBLE);
    return all_ok;
}

static int test_dsymv(void)
{
    int n = 64;
    double *A  = dalloc(n*n);
    double *x  = dalloc(n);
    double *y  = dalloc(n);
    double *yr = dalloc(n);

    printf(SEPARATOR);
    printf("TEST: DSYMV %dx%d\n", n, n);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            double v = (double)((i*n+j) % 17 - 8) * 0.1;
            A[i*n+j] = A[j*n+i] = v;
        }
        A[i*n+i] += (double)n;
    }
    dfill(x, n, 0.1, 0.05);
    for (int i = 0; i < n; i++) y[i] = yr[i] = 0.0;

    cblas_dsymv(CblasRowMajor, CblasUpper, n,
                1.0, A, n, x, 1, 0.0, y, 1);

    for (int i = 0; i < n; i++) {
        double acc = 0.0;
        for (int j = 0; j < n; j++)
            acc += A[i*n+j] * x[j];
        yr[i] = acc;
    }

    double err = dmax_err(y, yr, n);
    int ok = (err < MAX_ERR_DOUBLE);

    printf("  max_err=%.3e  %s\n", err, ok ? "OK" : "FAIL");
    record("DSYMV", ok, err, MAX_ERR_DOUBLE);

    free(A); free(x); free(y); free(yr);
    return ok;
}

static int test_dtrsv(void)
{
    int n = 32;
    double *A  = dalloc(n*n);
    double *x  = dalloc(n);
    double *xr = dalloc(n);

    printf(SEPARATOR);
    printf("TEST: DTRSV lower triangular solve %dx%d\n", n, n);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++)
            A[i*n+j] = (i == j) ? (double)(i+1) : (double)((i+j) % 5 - 2) * 0.1;
        for (int j = i+1; j < n; j++)
            A[i*n+j] = 0.0;
    }
    dfill(x, n, 1.0, 0.1);
    memcpy(xr, x, sizeof(double) * n);

    cblas_dtrsv(CblasRowMajor, CblasLower, CblasNoTrans, CblasNonUnit,
                n, A, n, x, 1);

    for (int i = 0; i < n; i++) {
        double sum = 0.0;
        for (int j = 0; j < i; j++)
            sum += A[i*n+j] * xr[j];
        xr[i] = (xr[i] - sum) / A[i*n+i];
    }

    double err = dmax_err(x, xr, n);
    int ok = (err < MAX_ERR_DOUBLE);

    printf("  max_err=%.3e  %s\n", err, ok ? "OK" : "FAIL");
    record("DTRSV", ok, err, MAX_ERR_DOUBLE);

    free(A); free(x); free(xr);
    return ok;
}

static int test_sgemm(void)
{
    int n = 64;
    float *A  = salloc(n*n);
    float *B  = salloc(n*n);
    float *C  = salloc(n*n);
    float *Cr = salloc(n*n);

    printf(SEPARATOR);
    printf("TEST: SGEMM %dx%d\n", n, n);

    for (int i = 0; i < n*n; i++) {
        A[i] = (float)(i % 11 - 5) * 0.1f;
        B[i] = (float)(i % 13 - 6) * 0.1f;
        C[i] = Cr[i] = 0.0f;
    }

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
    int ok = (max_err < MAX_ERR_FLOAT);

    printf("  max_err=%.3e  %s\n", (double)max_err, ok ? "OK" : "FAIL");
    record("SGEMM", ok, (double)max_err, MAX_ERR_FLOAT);

    free(A); free(B); free(C); free(Cr);
    return ok;
}

static void print_summary(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║              Numerical Suite Summary                  ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");

    int total_pass = 0, total_fail = 0;
    for (int i = 0; i < nresults; i++) {
        printf("║  %-22s  max_err=%-12.3e  %s  ║\n",
               results[i].name,
               results[i].max_err,
               results[i].passed ? "✅ PASS" : "❌ FAIL");
        if (results[i].passed) total_pass++;
        else                   total_fail++;
    }

    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║  Total: %2d passed, %2d failed                         ║\n",
           total_pass, total_fail);
    printf("╚══════════════════════════════════════════════════════╝\n");
}

int main(void)
{
    printf("RVForge Numerical Correctness Suite\n");
    printf("Target arch: %s\n",
#if defined(__riscv)
        "riscv64"
#elif defined(__x86_64__)
        "x86_64"
#else
        "unknown"
#endif
    );
    printf("\n");

    int rc = 0;
    rc |= !test_ddot_sizes();
    rc |= !test_dnrm2_sizes();
    rc |= !test_dgemv();
    rc |= !test_dgemm();
    rc |= !test_daxpy();
    rc |= !test_dsymv();
    rc |= !test_dtrsv();
    rc |= !test_sgemm();

    print_summary();

    printf("\nSTATUS: %s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}

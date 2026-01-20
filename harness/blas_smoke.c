#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cblas.h>

#define N 512

static double *alloc_matrix(int rows, int cols)
{
    double *m = (double *)malloc(sizeof(double) * rows * cols);
    if (!m) { fprintf(stderr, "malloc failed\n"); exit(1); }
    return m;
}

static void fill_matrix(double *m, int rows, int cols, double scale)
{
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            m[i * cols + j] = scale * ((double)(i - j) / (rows + cols));
}

static double frobenius_norm(const double *m, int rows, int cols)
{
    double s = 0.0;
    for (int i = 0; i < rows * cols; i++)
        s += m[i] * m[i];
    return sqrt(s);
}

static int verify_dgemm(int n)
{
    double *A = alloc_matrix(n, n);
    double *B = alloc_matrix(n, n);
    double *C = alloc_matrix(n, n);

    fill_matrix(A, n, n, 1.0);
    fill_matrix(B, n, n, 0.5);

    for (int i = 0; i < n * n; i++) C[i] = 0.0;

    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                n, n, n,
                1.0, A, n,
                     B, n,
                0.0, C, n);

    double norm = frobenius_norm(C, n, n);
    printf("DGEMM %dx%d Frobenius norm = %.10f\n", n, n, norm);

    free(A); free(B); free(C);
    return (norm > 0.0 && !isnan(norm) && !isinf(norm)) ? 0 : 1;
}

static int verify_ddot(int n)
{
    double *x = (double *)malloc(sizeof(double) * n);
    double *y = (double *)malloc(sizeof(double) * n);

    for (int i = 0; i < n; i++) {
        x[i] = (double)i / n;
        y[i] = 1.0 - (double)i / n;
    }

    double dot = cblas_ddot(n, x, 1, y, 1);
    double expected = 0.0;
    for (int i = 0; i < n; i++)
        expected += x[i] * y[i];

    printf("DDOT n=%d: got=%.10f expected=%.10f diff=%.3e\n",
           n, dot, expected, fabs(dot - expected));

    free(x); free(y);
    return (fabs(dot - expected) < 1e-9) ? 0 : 1;
}

static int verify_dnrm2(int n)
{
    double *x = (double *)malloc(sizeof(double) * n);
    for (int i = 0; i < n; i++) x[i] = 1.0 / sqrt((double)(i + 1));

    double nrm = cblas_dnrm2(n, x, 1);
    double expected = 0.0;
    for (int i = 0; i < n; i++) expected += x[i] * x[i];
    expected = sqrt(expected);

    printf("DNRM2 n=%d: got=%.10f expected=%.10f diff=%.3e\n",
           n, nrm, expected, fabs(nrm - expected));

    free(x);
    return (fabs(nrm - expected) < 1e-9) ? 0 : 1;
}

int main(void)
{
    printf("OpenBLAS smoke test — target: riscv64\n\n");

    int rc = 0;
    rc |= verify_ddot(N);
    rc |= verify_dnrm2(N);
    rc |= verify_dgemm(64);

    printf("\nSTATUS: %s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}

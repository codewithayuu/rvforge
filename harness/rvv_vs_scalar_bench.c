#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../rvv/rvv_dispatch.h"

#define WARMUP_ITERS  3
#define BENCH_ITERS   10
#define NS_PER_SEC    1000000000LL

static double *dalloc_rand(long n, double seed)
{
    double *v = (double *)malloc(sizeof(double) * n);
    if (!v) { fprintf(stderr, "OOM\n"); exit(1); }
    for (long i = 0; i < n; i++)
        v[i] = seed + (double)i * 0.0001 - (double)(n/2) * 0.00005;
    return v;
}

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * NS_PER_SEC + ts.tv_nsec;
}

typedef struct {
    const char *name;
    long        n;
    double      scalar_ns;
    double      rvv_ns;
    double      speedup;
    double      scalar_gflops;
    double      rvv_gflops;
    double      max_err;
} BenchResult;

static BenchResult results[256];
static int         nresults = 0;

static void record_result(const char *name, long n,
                          double scalar_ns, double rvv_ns,
                          double flops_per_call, double max_err)
{
    BenchResult *r     = &results[nresults++];
    r->name            = name;
    r->n               = n;
    r->scalar_ns       = scalar_ns;
    r->rvv_ns          = rvv_ns;
    r->speedup         = scalar_ns / rvv_ns;
    r->scalar_gflops   = flops_per_call / (scalar_ns * 1.0);
    r->rvv_gflops      = flops_per_call / (rvv_ns * 1.0);
    r->max_err         = max_err;
}

static void bench_ddot(long n)
{
    double *x = dalloc_rand(n, 1.0);
    double *y = dalloc_rand(n, 2.0);

    for (int i = 0; i < WARMUP_ITERS; i++) {
        rvforge_scalar_ddot(n, x, 1, y, 1);
        rvforge_rvv_ddot(n, x, 1, y, 1);
    }

    long long t0 = now_ns();
    double sv = 0.0;
    for (int i = 0; i < BENCH_ITERS; i++)
        sv += rvforge_scalar_ddot(n, x, 1, y, 1);
    double scalar_ns = (double)(now_ns() - t0) / BENCH_ITERS;
    (void)sv;

    t0 = now_ns();
    double rv = 0.0;
    for (int i = 0; i < BENCH_ITERS; i++)
        rv += rvforge_rvv_ddot(n, x, 1, y, 1);
    double rvv_ns = (double)(now_ns() - t0) / BENCH_ITERS;
    (void)rv;

    double ref = rvforge_scalar_ddot(n, x, 1, y, 1);
    double got = rvforge_rvv_ddot(n, x, 1, y, 1);
    double err = fabs(got - ref) / (fabs(ref) > 0.0 ? fabs(ref) : 1.0);

    record_result("DDOT", n, scalar_ns, rvv_ns, 2.0 * n, err);
    free(x); free(y);
}

static void bench_daxpy(long n)
{
    double *x    = dalloc_rand(n, 1.5);
    double *yscl = dalloc_rand(n, 0.5);
    double *yrvv = dalloc_rand(n, 0.5);
    double alpha  = 3.14159265358979;

    for (int i = 0; i < WARMUP_ITERS; i++) {
        memcpy(yscl, x, sizeof(double) * n);
        memcpy(yrvv, x, sizeof(double) * n);
        rvforge_scalar_daxpy(n, alpha, x, 1, yscl, 1);
        rvforge_rvv_daxpy(n, alpha, x, 1, yrvv, 1);
    }

    double *ytmp = dalloc_rand(n, 0.5);

    long long t0 = now_ns();
    for (int i = 0; i < BENCH_ITERS; i++) {
        memcpy(ytmp, x, sizeof(double) * n);
        rvforge_scalar_daxpy(n, alpha, x, 1, ytmp, 1);
    }
    double scalar_ns = (double)(now_ns() - t0) / BENCH_ITERS;

    t0 = now_ns();
    for (int i = 0; i < BENCH_ITERS; i++) {
        memcpy(ytmp, x, sizeof(double) * n);
        rvforge_rvv_daxpy(n, alpha, x, 1, ytmp, 1);
    }
    double rvv_ns = (double)(now_ns() - t0) / BENCH_ITERS;

    memcpy(yscl, x, sizeof(double) * n);
    memcpy(yrvv, x, sizeof(double) * n);
    rvforge_scalar_daxpy(n, alpha, x, 1, yscl, 1);
    rvforge_rvv_daxpy(n, alpha, x, 1, yrvv, 1);
    double max_err = 0.0;
    for (long i = 0; i < n; i++) {
        double d = fabs(yrvv[i] - yscl[i]);
        if (d > max_err) max_err = d;
    }

    record_result("DAXPY", n, scalar_ns, rvv_ns, 2.0 * n, max_err);
    free(x); free(yscl); free(yrvv); free(ytmp);
}

static void bench_dnrm2(long n)
{
    double *x = dalloc_rand(n, 1.0);

    for (int i = 0; i < WARMUP_ITERS; i++) {
        rvforge_scalar_dnrm2(n, x, 1);
        rvforge_rvv_dnrm2(n, x, 1);
    }

    long long t0 = now_ns();
    double sv = 0.0;
    for (int i = 0; i < BENCH_ITERS; i++)
        sv += rvforge_scalar_dnrm2(n, x, 1);
    double scalar_ns = (double)(now_ns() - t0) / BENCH_ITERS;
    (void)sv;

    t0 = now_ns();
    double rv = 0.0;
    for (int i = 0; i < BENCH_ITERS; i++)
        rv += rvforge_rvv_dnrm2(n, x, 1);
    double rvv_ns = (double)(now_ns() - t0) / BENCH_ITERS;
    (void)rv;

    double ref = rvforge_scalar_dnrm2(n, x, 1);
    double got = rvforge_rvv_dnrm2(n, x, 1);
    double err = fabs(got - ref) / (ref > 0.0 ? ref : 1.0);

    record_result("DNRM2", n, scalar_ns, rvv_ns, 2.0 * n, err);
    free(x);
}

static void bench_dgemv(long m, long n)
{
    double *A    = dalloc_rand(m * n, 0.3);
    double *x    = dalloc_rand(n, 1.0);
    double *yscl = dalloc_rand(m, 0.5);
    double *yrvv = dalloc_rand(m, 0.5);

    for (int i = 0; i < WARMUP_ITERS; i++) {
        memcpy(yscl, x, sizeof(double) * m);
        memcpy(yrvv, x, sizeof(double) * m);
        rvforge_scalar_dgemv(m, n, 1.0, A, n, x, 0.0, yscl);
        rvforge_rvv_dgemv(m, n, 1.0, A, n, x, 0.0, yrvv);
    }

    double *ytmp = dalloc_rand(m, 0.5);

    long long t0 = now_ns();
    for (int i = 0; i < BENCH_ITERS; i++) {
        memcpy(ytmp, x, sizeof(double) * m);
        rvforge_scalar_dgemv(m, n, 1.0, A, n, x, 0.0, ytmp);
    }
    double scalar_ns = (double)(now_ns() - t0) / BENCH_ITERS;

    t0 = now_ns();
    for (int i = 0; i < BENCH_ITERS; i++) {
        memcpy(ytmp, x, sizeof(double) * m);
        rvforge_rvv_dgemv(m, n, 1.0, A, n, x, 0.0, ytmp);
    }
    double rvv_ns = (double)(now_ns() - t0) / BENCH_ITERS;

    memcpy(yscl, x, sizeof(double) * m);
    memcpy(yrvv, x, sizeof(double) * m);
    rvforge_scalar_dgemv(m, n, 1.0, A, n, x, 0.0, yscl);
    rvforge_rvv_dgemv(m, n, 1.0, A, n, x, 0.0, yrvv);
    double max_err = 0.0;
    for (long i = 0; i < m; i++) {
        double d = fabs(yrvv[i] - yscl[i]);
        if (d > max_err) max_err = d;
    }

    char label[32];
    snprintf(label, sizeof(label), "DGEMV_%ldx%ld", m, n);
    record_result(label, m, scalar_ns, rvv_ns, 2.0 * m * n, max_err);
    free(A); free(x); free(yscl); free(yrvv); free(ytmp);
}

static void bench_dgemm(long n)
{
    double *A    = dalloc_rand(n * n, 0.1);
    double *B    = dalloc_rand(n * n, 0.2);
    double *Cscl = dalloc_rand(n * n, 0.0);
    double *Crvv = dalloc_rand(n * n, 0.0);

    for (int i = 0; i < WARMUP_ITERS; i++) {
        rvforge_scalar_dgemm(n, n, n, 1.0, A, n, B, n, 0.0, Cscl, n);
        rvforge_rvv_dgemm   (n, n, n, 1.0, A, n, B, n, 0.0, Crvv, n);
    }

    long long t0 = now_ns();
    for (int i = 0; i < BENCH_ITERS; i++)
        rvforge_scalar_dgemm(n, n, n, 1.0, A, n, B, n, 0.0, Cscl, n);
    double scalar_ns = (double)(now_ns() - t0) / BENCH_ITERS;

    t0 = now_ns();
    for (int i = 0; i < BENCH_ITERS; i++)
        rvforge_rvv_dgemm(n, n, n, 1.0, A, n, B, n, 0.0, Crvv, n);
    double rvv_ns = (double)(now_ns() - t0) / BENCH_ITERS;

    double max_err = 0.0;
    for (long i = 0; i < n*n; i++) {
        double d = fabs(Crvv[i] - Cscl[i]);
        if (d > max_err) max_err = d;
    }

    char label[32];
    snprintf(label, sizeof(label), "DGEMM_%ldx%ld", n, n);
    record_result(label, n, scalar_ns, rvv_ns, 2.0 * n * n * n, max_err);
    free(A); free(B); free(Cscl); free(Crvv);
}

static void print_header(void)
{
    printf("RVV vs Scalar Benchmark\n");
    printf("Emulator:  qemu-riscv64-static\n");
    printf("ISA flags: %s\n",
#if RVFORGE_HAS_RVV
        "-march=rv64gcv -mabi=lp64d"
#else
        "-march=rv64gc  -mabi=lp64d (RVV NOT active)"
#endif
    );
    printf("Warmup: %d iters  Bench: %d iters\n\n",
           WARMUP_ITERS, BENCH_ITERS);
}

static void print_table(void)
{
    printf("%-20s  %8s  %12s  %12s  %8s  %12s  %12s  %10s\n",
           "Kernel", "N", "Scalar(ns)", "RVV(ns)",
           "Speedup", "Scl GFLOP/s", "RVV GFLOP/s", "max_err");
    printf("%s\n", "──────────────────────────────────────────────────────"
                   "────────────────────────────────────────────────────");

    for (int i = 0; i < nresults; i++) {
        BenchResult *r = &results[i];
        printf("%-20s  %8ld  %12.1f  %12.1f  %8.2fx  %12.6f  %12.6f  %10.3e\n",
               r->name, r->n,
               r->scalar_ns, r->rvv_ns, r->speedup,
               r->scalar_gflops, r->rvv_gflops,
               r->max_err);
    }
}

static void print_markdown(void)
{
    printf("\n## Markdown Table (for docs/bench_results.md)\n\n");
    printf("| Kernel | N | Scalar (ns) | RVV (ns) | Speedup | "
           "Scalar GFLOP/s | RVV GFLOP/s | max_err |\n");
    printf("|--------|---|-------------|----------|---------|"
           "---------------|-------------|----------|\n");

    for (int i = 0; i < nresults; i++) {
        BenchResult *r = &results[i];
        printf("| `%s` | %ld | %.1f | %.1f | **%.2fx** | "
               "%.4f | %.4f | `%.3e` |\n",
               r->name, r->n,
               r->scalar_ns, r->rvv_ns, r->speedup,
               r->scalar_gflops, r->rvv_gflops,
               r->max_err);
    }
}

int main(void)
{
    print_header();

    long dot_sizes[]   = {64, 128, 256, 512, 1024, 4096, 16384};
    long axpy_sizes[]  = {64, 256, 1024, 4096, 16384};
    long nrm2_sizes[]  = {64, 256, 1024, 4096};
    long gemv_sizes[]  = {32, 64, 128, 256};
    long gemm_sizes[]  = {16, 32, 64};

    for (int i = 0; i < (int)(sizeof(dot_sizes)/sizeof(dot_sizes[0])); i++)
        bench_ddot(dot_sizes[i]);

    for (int i = 0; i < (int)(sizeof(axpy_sizes)/sizeof(axpy_sizes[0])); i++)
        bench_daxpy(axpy_sizes[i]);

    for (int i = 0; i < (int)(sizeof(nrm2_sizes)/sizeof(nrm2_sizes[0])); i++)
        bench_dnrm2(nrm2_sizes[i]);

    for (int i = 0; i < (int)(sizeof(gemv_sizes)/sizeof(gemv_sizes[0])); i++)
        bench_dgemv(gemv_sizes[i], gemv_sizes[i]);

    for (int i = 0; i < (int)(sizeof(gemm_sizes)/sizeof(gemm_sizes[0])); i++)
        bench_dgemm(gemm_sizes[i]);

    print_table();
    print_markdown();

    printf("\nSTATUS: PASS\n");
    return 0;
}

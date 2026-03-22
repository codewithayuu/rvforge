#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpfr.h>
#include "../validation/epsilon.h"
#include "../validation/output_capture.h"

#define PREC_LOW    64
#define PREC_MID    256
#define PREC_HIGH   1024
#define NTERMS      40

typedef struct {
    const char *name;
    int         passed;
} SuiteEntry;

static SuiteEntry suite[128];
static int        nsuite = 0;

static void record_suite(const char *name, int passed)
{
    suite[nsuite].name   = name;
    suite[nsuite].passed = passed;
    nsuite++;
}

static int test_const_pi(mpfr_prec_t prec)
{
    mpfr_t pi_a, pi_b, diff;
    mpfr_inits2(prec, pi_a, pi_b, diff, (mpfr_ptr)NULL);

    mpfr_const_pi(pi_a, MPFR_RNDN);
    mpfr_const_pi(pi_b, MPFR_RNDD);

    mpfr_sub(diff, pi_a, pi_b, MPFR_RNDN);
    mpfr_abs(diff, diff, MPFR_RNDN);
    double err = mpfr_get_d(diff, MPFR_RNDN);

    char buf[256];
    mpfr_snprintf(buf, sizeof(buf), "%.50Rf", pi_a);
    printf("  pi(%4lu bits) = %s\n", (unsigned long)prec, buf);
    printf("  rounding_err = %.6e\n", err);

    int ok = (err < 1e-15);
    mpfr_clears(pi_a, pi_b, diff, (mpfr_ptr)NULL);
    return ok;
}

static int test_const_log2(mpfr_prec_t prec)
{
    mpfr_t log2_a, log2_ref, diff;
    mpfr_inits2(prec, log2_a, log2_ref, diff, (mpfr_ptr)NULL);

    mpfr_const_log2(log2_a, MPFR_RNDN);

    mpfr_set_ui(log2_ref, 2, MPFR_RNDN);
    mpfr_log(log2_ref, log2_ref, MPFR_RNDN);

    mpfr_sub(diff, log2_a, log2_ref, MPFR_RNDN);
    mpfr_abs(diff, diff, MPFR_RNDN);
    double err = mpfr_get_d(diff, MPFR_RNDN);

    char buf[256];
    mpfr_snprintf(buf, sizeof(buf), "%.50Rf", log2_a);
    printf("  log2(%4lu bits) = %s\n", (unsigned long)prec, buf);
    printf("  err_vs_mpfr_log = %.6e\n", err);

    int ok = (err < 1e-15);
    mpfr_clears(log2_a, log2_ref, diff, (mpfr_ptr)NULL);
    return ok;
}

static int test_exp_series_vs_builtin(void)
{
    mpfr_prec_t prec = PREC_MID;
    mpfr_t x, series_result, builtin_result, term, xpow, fact, diff;
    mpfr_inits2(prec,
                x, series_result, builtin_result,
                term, xpow, fact, diff,
                (mpfr_ptr)NULL);

    static const double xvals[] = {0.0, 0.5, 1.0, -1.0, 2.0, -0.1, 10.0};
    int nvals = (int)(sizeof(xvals) / sizeof(xvals[0]));
    int all_ok = 1;

    for (int v = 0; v < nvals; v++) {
        mpfr_set_d(x, xvals[v], MPFR_RNDN);
        mpfr_exp(builtin_result, x, MPFR_RNDN);

        mpfr_set_ui(series_result, 0, MPFR_RNDN);
        mpfr_set_ui(term, 1, MPFR_RNDN);
        mpfr_set_ui(xpow, 1, MPFR_RNDN);
        mpfr_set_ui(fact, 1, MPFR_RNDN);

        for (int n = 0; n < NTERMS; n++) {
            mpfr_add(series_result, series_result, term, MPFR_RNDN);
            mpfr_mul(xpow, xpow, x, MPFR_RNDN);
            mpfr_mul_ui(fact, fact, (unsigned long)(n + 1), MPFR_RNDN);
            mpfr_div(term, xpow, fact, MPFR_RNDN);
        }

        mpfr_sub(diff, series_result, builtin_result, MPFR_RNDN);
        mpfr_abs(diff, diff, MPFR_RNDN);
        double err = mpfr_get_d(diff, MPFR_RNDN);

        int ok = (err < RVFORGE_EPS_MPFR);
        if (!ok) all_ok = 0;
        printf("  exp(%.2f): err=%.6e  %s\n", xvals[v], err, ok ? "OK" : "FAIL");
    }

    mpfr_clears(x, series_result, builtin_result, term, xpow, fact, diff,
                (mpfr_ptr)NULL);
    return all_ok;
}

static int test_sqrt_newton(void)
{
    mpfr_prec_t prec = PREC_MID;
    mpfr_t x, sqrt_mpfr, sqrt_newton, half, diff;
    mpfr_inits2(prec, x, sqrt_mpfr, sqrt_newton, half, diff, (mpfr_ptr)NULL);
    mpfr_set_d(half, 0.5, MPFR_RNDN);

    static const double vals[] = {2.0, 3.0, 5.0, 0.5, 100.0, 1e-10, 1e10};
    int nvals = (int)(sizeof(vals) / sizeof(vals[0]));
    int all_ok = 1;

    for (int v = 0; v < nvals; v++) {
        mpfr_set_d(x, vals[v], MPFR_RNDN);
        mpfr_sqrt(sqrt_mpfr, x, MPFR_RNDN);

        mpfr_set_d(sqrt_newton, vals[v], MPFR_RNDN);
        for (int iter = 0; iter < 60; iter++) {
            mpfr_t tmp;
            mpfr_init2(tmp, prec);
            mpfr_div(tmp, x, sqrt_newton, MPFR_RNDN);
            mpfr_add(sqrt_newton, sqrt_newton, tmp, MPFR_RNDN);
            mpfr_mul(sqrt_newton, sqrt_newton, half, MPFR_RNDN);
            mpfr_clear(tmp);
        }

        mpfr_sub(diff, sqrt_newton, sqrt_mpfr, MPFR_RNDN);
        mpfr_abs(diff, diff, MPFR_RNDN);
        double err = mpfr_get_d(diff, MPFR_RNDN);

        int ok = (err < RVFORGE_EPS_MPFR);
        if (!ok) all_ok = 0;
        printf("  sqrt(%.2e): err=%.6e  %s\n", vals[v], err, ok ? "OK" : "FAIL");
    }

    mpfr_clears(x, sqrt_mpfr, sqrt_newton, half, diff, (mpfr_ptr)NULL);
    return all_ok;
}

static int test_trig_identities(void)
{
    mpfr_prec_t prec = PREC_MID;
    mpfr_t x, s, c, s2, c2, one, diff;
    mpfr_inits2(prec, x, s, c, s2, c2, one, diff, (mpfr_ptr)NULL);
    mpfr_set_ui(one, 1, MPFR_RNDN);

    static const double angles[] = {
        0.0, 0.1, 0.5, 1.0, 1.5707963, 2.0, 3.14159265, -1.0, 100.0
    };
    int nangles = (int)(sizeof(angles) / sizeof(angles[0]));
    int all_ok = 1;

    for (int a = 0; a < nangles; a++) {
        mpfr_set_d(x, angles[a], MPFR_RNDN);
        mpfr_sin(s, x, MPFR_RNDN);
        mpfr_cos(c, x, MPFR_RNDN);

        mpfr_mul(s2, s, s, MPFR_RNDN);
        mpfr_mul(c2, c, c, MPFR_RNDN);
        mpfr_add(s2, s2, c2, MPFR_RNDN);
        mpfr_sub(diff, s2, one, MPFR_RNDN);
        mpfr_abs(diff, diff, MPFR_RNDN);
        double err = mpfr_get_d(diff, MPFR_RNDN);

        int ok = (err < 1e-60);
        if (!ok) all_ok = 0;
        printf("  sin²+cos²(%.4f): err=%.6e  %s\n", angles[a], err,
               ok ? "OK" : "FAIL");
    }

    mpfr_clears(x, s, c, s2, c2, one, diff, (mpfr_ptr)NULL);
    return all_ok;
}

static int test_precision_levels(void)
{
    mpfr_prec_t precs[] = {32, 64, 128, 256, 512, 1024};
    int nprecs = (int)(sizeof(precs) / sizeof(precs[0]));
    int all_ok = 1;

    for (int p = 0; p < nprecs; p++) {
        mpfr_prec_t prec = precs[p];
        mpfr_t a, b, c;
        mpfr_inits2(prec, a, b, c, (mpfr_ptr)NULL);

        mpfr_set_ui(a, 1, MPFR_RNDN);
        mpfr_set_ui(b, 3, MPFR_RNDN);
        mpfr_div(c, a, b, MPFR_RNDN);
        mpfr_mul_ui(c, c, 3, MPFR_RNDN);
        mpfr_sub(c, c, a, MPFR_RNDN);
        mpfr_abs(c, c, MPFR_RNDN);
        double err = mpfr_get_d(c, MPFR_RNDN);

        int ok = (err < pow(2.0, -(double)(prec - 4)));
        if (!ok) all_ok = 0;
        printf("  (1/3)*3-1 at prec=%4lu bits: err=%.6e  %s\n",
               (unsigned long)prec, err, ok ? "OK" : "FAIL");

        mpfr_clears(a, b, c, (mpfr_ptr)NULL);
    }

    return all_ok;
}

static int test_rounding_modes(void)
{
    mpfr_prec_t prec = 64;
    mpfr_t a, b;
    mpfr_inits2(prec, a, b, (mpfr_ptr)NULL);
    mpfr_set_d(a, 1.0, MPFR_RNDN);
    mpfr_set_d(b, 3.0, MPFR_RNDN);

    mpfr_t rn, ru, rd, rz;
    mpfr_inits2(prec, rn, ru, rd, rz, (mpfr_ptr)NULL);

    mpfr_div(rn, a, b, MPFR_RNDN);
    mpfr_div(ru, a, b, MPFR_RNDU);
    mpfr_div(rd, a, b, MPFR_RNDD);
    mpfr_div(rz, a, b, MPFR_RNDZ);

    double vn = mpfr_get_d(rn, MPFR_RNDN);
    double vu = mpfr_get_d(ru, MPFR_RNDN);
    double vd = mpfr_get_d(rd, MPFR_RNDN);
    double vz = mpfr_get_d(rz, MPFR_RNDN);

    printf("  1/3 RNDN=%.20f\n", vn);
    printf("  1/3 RNDU=%.20f\n", vu);
    printf("  1/3 RNDD=%.20f\n", vd);
    printf("  1/3 RNDZ=%.20f\n", vz);

    int ok = (vu > vd) && (vn >= vd) && (vn <= vu) && (vz == vd);
    printf("  Rounding order correct: %s\n", ok ? "OK" : "FAIL");

    mpfr_clears(a, b, rn, ru, rd, rz, (mpfr_ptr)NULL);
    return ok;
}

static int test_special_values(void)
{
    mpfr_t inf_val, nan_val, zero_val, result;
    mpfr_inits2(PREC_MID, inf_val, nan_val, zero_val, result, (mpfr_ptr)NULL);

    mpfr_set_inf(inf_val,  1);
    mpfr_set_nan(nan_val);
    mpfr_set_zero(zero_val, 1);

    int ok = 1;
    ok &= mpfr_inf_p(inf_val)  != 0;
    ok &= mpfr_nan_p(nan_val)  != 0;
    ok &= mpfr_zero_p(zero_val) != 0;

    mpfr_add(result, inf_val, zero_val, MPFR_RNDN);
    ok &= mpfr_inf_p(result) != 0;

    printf("  inf detected:  %s\n", mpfr_inf_p(inf_val)   ? "OK" : "FAIL");
    printf("  nan detected:  %s\n", mpfr_nan_p(nan_val)   ? "OK" : "FAIL");
    printf("  zero detected: %s\n", mpfr_zero_p(zero_val) ? "OK" : "FAIL");
    printf("  inf+0=inf:     %s\n", mpfr_inf_p(result)    ? "OK" : "FAIL");

    mpfr_clears(inf_val, nan_val, zero_val, result, (mpfr_ptr)NULL);
    return ok;
}

static void print_suite_summary(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║           MPFR Full Suite Summary                     ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");
    int pass = 0, fail = 0;
    for (int i = 0; i < nsuite; i++) {
        printf("║  %-38s  %s  ║\n",
               suite[i].name,
               suite[i].passed ? "✅ PASS" : "❌ FAIL");
        if (suite[i].passed) pass++;
        else                 fail++;
    }
    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║  Total: %d passed, %d failed                           ║\n",
           pass, fail);
    printf("╚══════════════════════════════════════════════════════╝\n");
}

int main(void)
{
    printf("MPFR Full Validation Suite\n");
    printf("MPFR version: %s\n", mpfr_get_version());
    printf("GMP  version: %s\n", gmp_version);
    printf("Target: %s\n\n",
#if defined(__riscv)
        "riscv64"
#else
        "x86_64"
#endif
    );

    int rc = 0;

    printf("--- Constants (multi-precision) ---\n");
    {
        int ok = test_const_pi(PREC_LOW) &
                 test_const_pi(PREC_MID) &
                 test_const_pi(PREC_HIGH);
        record_suite("const_pi", ok);
        rc |= !ok;
    }

    {
        int ok = test_const_log2(PREC_MID);
        record_suite("const_log2", ok);
        rc |= !ok;
    }

    printf("\n--- Elementary Functions ---\n");
    {
        int ok = test_exp_series_vs_builtin();
        record_suite("exp_series", ok);
        rc |= !ok;
    }

    {
        int ok = test_sqrt_newton();
        record_suite("sqrt_newton", ok);
        rc |= !ok;
    }

    {
        int ok = test_trig_identities();
        record_suite("trig_identities", ok);
        rc |= !ok;
    }

    printf("\n--- Precision Levels ---\n");
    {
        int ok = test_precision_levels();
        record_suite("precision_levels", ok);
        rc |= !ok;
    }

    printf("\n--- Rounding Modes ---\n");
    {
        int ok = test_rounding_modes();
        record_suite("rounding_modes", ok);
        rc |= !ok;
    }

    printf("\n--- Special Values ---\n");
    {
        int ok = test_special_values();
        record_suite("special_values", ok);
        rc |= !ok;
    }

    print_suite_summary();

    printf("\nSTATUS: %s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}

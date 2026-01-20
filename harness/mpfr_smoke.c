#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpfr.h>

#define PRECISION 256
#define NTERMS    20

static void compute_pi_bbp(mpfr_t result, mpfr_prec_t prec)
{
    mpfr_t term, k8, tmp1, tmp2, pow16;
    mpfr_inits2(prec, term, k8, tmp1, tmp2, pow16, (mpfr_ptr)NULL);
    mpfr_set_ui(result, 0, MPFR_RNDN);
    mpfr_set_ui(pow16, 1, MPFR_RNDN);

    for (unsigned long k = 0; k < NTERMS; k++) {
        mpfr_set_ui(k8, 8 * k, MPFR_RNDN);

        mpfr_set_ui(tmp1, 4, MPFR_RNDN);
        mpfr_add_ui(tmp2, k8, 1, MPFR_RNDN);
        mpfr_div(tmp1, tmp1, tmp2, MPFR_RNDN);

        mpfr_set_ui(tmp2, 2, MPFR_RNDN);
        mpfr_add_ui(tmp2, k8, 4, MPFR_RNDN);
        mpfr_ui_div(tmp2, 2, tmp2, MPFR_RNDN);
        mpfr_sub(tmp1, tmp1, tmp2, MPFR_RNDN);

        mpfr_set_ui(tmp2, 1, MPFR_RNDN);
        mpfr_add_ui(tmp2, k8, 5, MPFR_RNDN);
        mpfr_ui_div(tmp2, 1, tmp2, MPFR_RNDN);
        mpfr_sub(tmp1, tmp1, tmp2, MPFR_RNDN);

        mpfr_set_ui(tmp2, 1, MPFR_RNDN);
        mpfr_add_ui(tmp2, k8, 6, MPFR_RNDN);
        mpfr_ui_div(tmp2, 1, tmp2, MPFR_RNDN);
        mpfr_sub(tmp1, tmp1, tmp2, MPFR_RNDN);

        mpfr_div(term, tmp1, pow16, MPFR_RNDN);
        mpfr_add(result, result, term, MPFR_RNDN);
        mpfr_mul_ui(pow16, pow16, 16, MPFR_RNDN);
    }

    mpfr_clears(term, k8, tmp1, tmp2, pow16, (mpfr_ptr)NULL);
}

static void compute_exp_series(mpfr_t result, double x_val, mpfr_prec_t prec)
{
    mpfr_t x, term, factorial;
    mpfr_inits2(prec, x, term, factorial, (mpfr_ptr)NULL);
    mpfr_set_d(x, x_val, MPFR_RNDN);
    mpfr_set_ui(result, 0, MPFR_RNDN);
    mpfr_set_ui(term, 1, MPFR_RNDN);
    mpfr_set_ui(factorial, 1, MPFR_RNDN);

    for (unsigned long n = 0; n < NTERMS; n++) {
        mpfr_add(result, result, term, MPFR_RNDN);
        mpfr_mul(term, term, x, MPFR_RNDN);
        mpfr_mul_ui(factorial, factorial, n + 1, MPFR_RNDN);
        mpfr_div(term, term, factorial, MPFR_RNDN);
    }

    mpfr_clears(x, term, factorial, (mpfr_ptr)NULL);
}

int main(void)
{
    printf("MPFR version: %s\n", mpfr_get_version());
    printf("GMP  version: %s\n", gmp_version);
    printf("Precision: %d bits\n\n", PRECISION);

    mpfr_t pi, ref, exp_result;
    mpfr_inits2(PRECISION, pi, ref, exp_result, (mpfr_ptr)NULL);

    compute_pi_bbp(pi, PRECISION);
    mpfr_const_pi(ref, MPFR_RNDN);

    char pi_buf[128], ref_buf[128];
    mpfr_snprintf(pi_buf,  sizeof(pi_buf),  "%.40Rf", pi);
    mpfr_snprintf(ref_buf, sizeof(ref_buf), "%.40Rf", ref);

    printf("BBP  pi = %s\n", pi_buf);
    printf("MPFR pi = %s\n", ref_buf);

    mpfr_sub(ref, pi, ref, MPFR_RNDN);
    mpfr_abs(ref, ref, MPFR_RNDN);
    double err = mpfr_get_d(ref, MPFR_RNDN);
    printf("Error   = %.6e\n\n", err);

    if (err > 1e-10) {
        fprintf(stderr, "FAIL: pi error exceeds threshold\n");
        mpfr_clears(pi, ref, exp_result, (mpfr_ptr)NULL);
        return 1;
    }

    compute_exp_series(exp_result, 1.0, PRECISION);
    char exp_buf[128];
    mpfr_snprintf(exp_buf, sizeof(exp_buf), "%.40Rf", exp_result);
    printf("exp(1) series = %s\n", exp_buf);

    mpfr_set_d(ref, 2.718281828459045235360287471352662497757, MPFR_RNDN);
    mpfr_sub(ref, exp_result, ref, MPFR_RNDN);
    mpfr_abs(ref, ref, MPFR_RNDN);
    err = mpfr_get_d(ref, MPFR_RNDN);
    printf("Error  = %.6e\n\n", err);

    if (err > 1e-10) {
        fprintf(stderr, "FAIL: exp(1) error exceeds threshold\n");
        mpfr_clears(pi, ref, exp_result, (mpfr_ptr)NULL);
        return 1;
    }

    printf("STATUS: PASS\n");
    mpfr_clears(pi, ref, exp_result, (mpfr_ptr)NULL);
    return 0;
}

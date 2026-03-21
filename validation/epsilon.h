#ifndef RVFORGE_EPSILON_H
#define RVFORGE_EPSILON_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RVFORGE_EPS_DOUBLE   1e-10
#define RVFORGE_EPS_FLOAT    1e-5
#define RVFORGE_EPS_MPFR     1e-30
#define RVFORGE_EPS_RELATIVE 1e-12

typedef enum {
    CMP_ABSOLUTE = 0,
    CMP_RELATIVE,
    CMP_ULP,
    CMP_COMBINED
} EpsilonMode;

typedef struct {
    const char *test_name;
    double      computed;
    double      reference;
    double      epsilon;
    EpsilonMode mode;
    int         passed;
    double      actual_error;
} EpsilonResult;

static inline double rvforge_abs_err(double a, double b)
{
    double d = a - b;
    return d < 0.0 ? -d : d;
}

static inline double rvforge_rel_err(double a, double b)
{
    if (b == 0.0) return (a == 0.0) ? 0.0 : 1.0;
    double d = (a - b) / b;
    return d < 0.0 ? -d : d;
}

static inline uint64_t rvforge_double_bits(double v)
{
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return bits;
}

static inline uint64_t rvforge_ulp_distance(double a, double b)
{
    uint64_t ba = rvforge_double_bits(a);
    uint64_t bb = rvforge_double_bits(b);
    if ((ba >> 63) != (bb >> 63)) {
        if (a == b) return 0;
        return UINT64_MAX;
    }
    return (ba > bb) ? (ba - bb) : (bb - ba);
}

static inline EpsilonResult rvforge_check_double(
    const char *name,
    double computed,
    double reference,
    double epsilon,
    EpsilonMode mode)
{
    EpsilonResult r;
    r.test_name  = name;
    r.computed   = computed;
    r.reference  = reference;
    r.epsilon    = epsilon;
    r.mode       = mode;

    switch (mode) {
    case CMP_ABSOLUTE:
        r.actual_error = rvforge_abs_err(computed, reference);
        r.passed = (r.actual_error <= epsilon);
        break;
    case CMP_RELATIVE:
        r.actual_error = rvforge_rel_err(computed, reference);
        r.passed = (r.actual_error <= epsilon);
        break;
    case CMP_ULP:
        r.actual_error = (double)rvforge_ulp_distance(computed, reference);
        r.passed = (r.actual_error <= epsilon);
        break;
    case CMP_COMBINED:
        {
            double abs_e = rvforge_abs_err(computed, reference);
            double rel_e = rvforge_rel_err(computed, reference);
            r.actual_error = (abs_e < rel_e) ? abs_e : rel_e;
            r.passed = (abs_e <= epsilon || rel_e <= RVFORGE_EPS_RELATIVE);
        }
        break;
    default:
        r.actual_error = rvforge_abs_err(computed, reference);
        r.passed = (r.actual_error <= epsilon);
        break;
    }

    return r;
}

static inline void rvforge_print_result(const EpsilonResult *r)
{
    const char *mode_str[] = {"ABS", "REL", "ULP", "COMB"};
    printf("  [%s][%s] computed=%.15e  ref=%.15e  err=%.3e  eps=%.3e  %s\n",
           r->test_name,
           mode_str[r->mode],
           r->computed,
           r->reference,
           r->actual_error,
           r->epsilon,
           r->passed ? "PASS" : "FAIL");
}

static inline int rvforge_check_array(
    const char  *name,
    const double *computed,
    const double *reference,
    int           n,
    double        epsilon,
    EpsilonMode   mode)
{
    int    all_pass = 1;
    double max_err  = 0.0;
    int    max_idx  = 0;

    for (int i = 0; i < n; i++) {
        EpsilonResult r = rvforge_check_double(name, computed[i],
                                               reference[i], epsilon, mode);
        if (!r.passed) all_pass = 0;
        if (r.actual_error > max_err) {
            max_err = r.actual_error;
            max_idx = i;
        }
    }

    printf("  [%s] n=%d  max_err=%.3e @ idx=%d  eps=%.3e  %s\n",
           name, n, max_err, max_idx, epsilon,
           all_pass ? "PASS" : "FAIL");

    return all_pass;
}

static inline int rvforge_check_array_f(
    const char *name,
    const float *computed,
    const float *reference,
    int          n,
    float        epsilon)
{
    int   all_pass = 1;
    float max_err  = 0.0f;
    int   max_idx  = 0;

    for (int i = 0; i < n; i++) {
        float d = computed[i] - reference[i];
        if (d < 0.0f) d = -d;
        if (!all_pass || d > max_err) {
            max_err = d;
            max_idx = i;
        }
        if (d > epsilon) all_pass = 0;
    }

    printf("  [%s] n=%d  max_err=%.3e @ idx=%d  eps=%.3e  %s\n",
           name, n, (double)max_err, max_idx, (double)epsilon,
           all_pass ? "PASS" : "FAIL");

    return all_pass;
}

#define RVFORGE_ASSERT_CLOSE(computed, reference, eps) \
    do { \
        EpsilonResult _r = rvforge_check_double( \
            #computed, (computed), (reference), (eps), CMP_ABSOLUTE); \
        rvforge_print_result(&_r); \
        if (!_r.passed) { \
            fprintf(stderr, "ASSERTION FAILED: %s:%d\n", __FILE__, __LINE__); \
            return 1; \
        } \
    } while(0)

#define RVFORGE_ASSERT_CLOSE_REL(computed, reference, eps) \
    do { \
        EpsilonResult _r = rvforge_check_double( \
            #computed, (computed), (reference), (eps), CMP_RELATIVE); \
        rvforge_print_result(&_r); \
        if (!_r.passed) { \
            fprintf(stderr, "ASSERTION FAILED: %s:%d\n", __FILE__, __LINE__); \
            return 1; \
        } \
    } while(0)

#endif /* RVFORGE_EPSILON_H */

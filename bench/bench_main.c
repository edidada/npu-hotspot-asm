/* =============================================================================
 * bench_main.c - throughput benchmark + performance gate.
 *
 * Uses the portable C11 timespec_get() so it builds on both glibc (Linux CI)
 * and MinGW (local Windows) without extra linkage.
 *
 * Measures the optimized backend against the scalar reference oracle and
 * enforces a minimum speedup. The reference kernel is compiled with
 * auto-vectorization disabled (see CMakeLists) so the comparison reflects a
 * fair scalar baseline rather than the compiler also emitting SIMD.
 *
 * Gate policy:
 *   - If AVX2 is unavailable, the optimized path IS the reference, so the
 *     gate is skipped (nothing meaningful to compare) and we just report.
 *   - Otherwise a speedup below NPU_MIN_SPEEDUP (default 2.0, overridable via
 *     env) is treated as a regression and fails the build.
 * ========================================================================== */
#include "npu_isa.h"
#include "npu_ref.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static double now_sec(void)
{
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static double bench_vmac(const int16_t *a, const int16_t *b, const int16_t *c,
                         int16_t *out, size_t n, int iters,
                         void (*fn)(const int16_t *, const int16_t *,
                                    const int16_t *, int16_t *, size_t))
{
    double s = now_sec();
    for (int k = 0; k < iters; k++) fn(a, b, c, out, n);
    return (now_sec() - s) / (double)iters;
}

static double bench_vadd(const int16_t *a, const int16_t *b, int16_t *out,
                         size_t n, int iters,
                         void (*fn)(const int16_t *, const int16_t *,
                                    int16_t *, size_t))
{
    double s = now_sec();
    for (int k = 0; k < iters; k++) fn(a, b, out, n);
    return (now_sec() - s) / (double)iters;
}

int main(void)
{
    const size_t N = 1u << 20;          /* 1M elements */
    const int ITERS = 30;
    int16_t *a = malloc(N * sizeof(int16_t));
    int16_t *b = malloc(N * sizeof(int16_t));
    int16_t *c = malloc(N * sizeof(int16_t));
    int16_t *out = malloc(N * sizeof(int16_t));
    for (size_t i = 0; i < N; i++) {
        a[i] = (int16_t)(rand() & 0xFFFF);
        b[i] = (int16_t)(rand() & 0xFFFF);
        c[i] = (int16_t)(rand() & 0xFFFF);
    }

    int has_avx2 = npu_hw_has_avx2();
    printf("NPU hotspot benchmark (AVX2 present: %s)\n", has_avx2 ? "yes" : "no");

    double t_ref, t_opt;
    int rc = 0;

    t_ref = bench_vmac(a, b, c, out, N, ITERS, npu_ref_vmac_sat_i16_vec);
    t_opt = bench_vmac(a, b, c, out, N, ITERS, npu_vmac_sat_i16);
    double sp_vmac = t_ref / t_opt;
    printf("  vmac.sat.i16  ref=%6.3f ms  opt=%6.3f ms  speedup=%.2fx\n",
           t_ref * 1e3, t_opt * 1e3, sp_vmac);

    t_ref = bench_vadd(a, b, out, N, ITERS, npu_ref_vadd_sat_i16_vec);
    t_opt = bench_vadd(a, b, out, N, ITERS, npu_vadd_sat_i16);
    double sp_vadd = t_ref / t_opt;
    printf("  vadd.sat.i16  ref=%6.3f ms  opt=%6.3f ms  speedup=%.2fx\n",
           t_ref * 1e3, t_opt * 1e3, sp_vadd);

    const char *env = getenv("NPU_MIN_SPEEDUP");
    double min_sp = (env != NULL) ? atof(env) : 2.0;

    if (!has_avx2) {
        printf("  (AVX2 unavailable: optimized path == reference; gate skipped)\n");
    } else if (sp_vmac < min_sp || sp_vadd < min_sp) {
        printf("PERF GATE: FAIL (required min speedup %.2fx)\n", min_sp);
        rc = 1;
    } else {
        printf("PERF GATE: PASS (min required %.2fx)\n", min_sp);
    }

    free(a); free(b); free(c); free(out);
    return rc;
}

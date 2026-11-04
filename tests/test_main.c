/* =============================================================================
 * test_main.c - bit-exact verification harness.
 *
 * Every optimized backend (dispatch, intrinsics, hand-written assembly) is
 * checked against the scalar reference oracle for:
 *   - explicit saturation / overflow edge cases
 *   - randomized fuzzing across many lengths (including non-multiples of 16)
 *
 * The AVX2 intrinsics/asm backends are only exercised when the CPU actually
 * supports AVX2 (npu_hw_has_avx2()); executing AVX2 without hardware faults.
 * ========================================================================== */
#include "npu_isa.h"
#include "npu_ref.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static int g_fail = 0;
static long g_checks = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                       \
            printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);         \
            g_fail++;                                                        \
        }                                                                    \
    } while (0)

static int16_t rnd_i16(void)
{
    return (int16_t)(rand() & 0xFFFF);
}

static void check_vmac(const char *name, const int16_t *a, const int16_t *b,
                       const int16_t *c, const int16_t *got, size_t n)
{
    int16_t *ref = (int16_t *)malloc(n ? n * sizeof(int16_t) : 1);
    npu_ref_vmac_sat_i16_vec(a, b, c, ref, n);
    for (size_t i = 0; i < n; i++) {
        if (got[i] != ref[i]) {
            printf("  FAIL[%s]: idx %zu got %d ref %d (a=%d b=%d c=%d)\n",
                   name, i, (int)got[i], (int)ref[i],
                   (int)a[i], (int)b[i], (int)c[i]);
            g_fail++;
            break;
        }
    }
    g_checks++;
    free(ref);
}

static void check_vadd(const char *name, const int16_t *a, const int16_t *b,
                       const int16_t *got, size_t n)
{
    int16_t *ref = (int16_t *)malloc(n ? n * sizeof(int16_t) : 1);
    npu_ref_vadd_sat_i16_vec(a, b, ref, n);
    for (size_t i = 0; i < n; i++) {
        if (got[i] != ref[i]) {
            printf("  FAIL[%s]: idx %zu got %d ref %d (a=%d b=%d)\n",
                   name, i, (int)got[i], (int)ref[i],
                   (int)a[i], (int)b[i]);
            g_fail++;
            break;
        }
    }
    g_checks++;
    free(ref);
}

static void test_edge(void)
{
    printf("[edge] saturation / overflow cases\n");
    {
        int16_t a[1], b[1], c[1], out[1];
        a[0] = 32767;  b[0] = 32767;  c[0] = 0;
        npu_vmac_sat_i16(a, b, c, out, 1);
        CHECK(out[0] == 32767, "vmac 32767*32767 sat high");
        a[0] = -32768; b[0] = -32768; c[0] = 0;
        npu_vmac_sat_i16(a, b, c, out, 1);
        CHECK(out[0] == 32767, "vmac (-32768)^2 sat high");
        a[0] = -300;   b[0] = 200;    c[0] = 0;
        npu_vmac_sat_i16(a, b, c, out, 1);
        CHECK(out[0] == -32768, "vmac negative overflow sat low");
        a[0] = 200;    b[0] = 200;    c[0] = 1000;
        npu_vmac_sat_i16(a, b, c, out, 1);
        CHECK(out[0] == 32767, "vmac +c overflow sat high");
        a[0] = 5;      b[0] = 6;      c[0] = -7;
        npu_vmac_sat_i16(a, b, c, out, 1);
        CHECK(out[0] == 23, "vmac in-range exact");
    }
    {
        int16_t a[1], b[1], out[1];
        a[0] = 32767;  b[0] = 1;
        npu_vadd_sat_i16(a, b, out, 1);
        CHECK(out[0] == 32767, "vadd high sat");
        a[0] = -32768; b[0] = -1;
        npu_vadd_sat_i16(a, b, out, 1);
        CHECK(out[0] == -32768, "vadd low sat");
        a[0] = 100;    b[0] = -30;
        npu_vadd_sat_i16(a, b, out, 1);
        CHECK(out[0] == 70, "vadd in-range exact");
    }
}

static void test_fuzz(void)
{
    printf("[fuzz] randomized over varied lengths\n");
    static const size_t lens[] = {0, 1, 2, 15, 16, 17, 31, 32, 33, 100, 1000, 50000};
    int16_t *a = malloc(50000 * sizeof(int16_t));
    int16_t *b = malloc(50000 * sizeof(int16_t));
    int16_t *c = malloc(50000 * sizeof(int16_t));
    int16_t *out = malloc(50000 * sizeof(int16_t));
    int16_t *out2 = malloc(50000 * sizeof(int16_t));
    int avx2 = npu_hw_has_avx2();

    for (size_t li = 0; li < sizeof(lens) / sizeof(lens[0]); li++) {
        size_t n = lens[li];
        for (int rep = 0; rep < 8; rep++) {
            for (size_t i = 0; i < n; i++) {
                a[i] = rnd_i16();
                b[i] = rnd_i16();
                c[i] = rnd_i16();
            }
            npu_vmac_sat_i16(a, b, c, out, n);
            check_vmac("dispatch.vmac", a, b, c, out, n);
            npu_vadd_sat_i16(a, b, out2, n);
            check_vadd("dispatch.vadd", a, b, out2, n);

            if (avx2) {
                npu_vmac_sat_i16_avx2_intrin(a, b, c, out, n);
                check_vmac("intrin.vmac", a, b, c, out, n);
                npu_vadd_sat_i16_avx2_intrin(a, b, out2, n);
                check_vadd("intrin.vadd", a, b, out2, n);
            }

#if defined(NPU_USE_ASM_KERNELS)
            if (avx2) {
                npu_vmac_sat_i16_avx2(a, b, c, out, n);
                check_vmac("asm.vmac", a, b, c, out, n);
                npu_vadd_sat_i16_avx2(a, b, out2, n);
                check_vadd("asm.vadd", a, b, out2, n);
            }
#endif
        }
    }
    free(a); free(b); free(c); free(out); free(out2);
}

int main(void)
{
    printf("NPU hotspot verification  (AVX2 present: %s)\n",
           npu_hw_has_avx2() ? "yes" : "no");
    test_edge();
    test_fuzz();
    printf("\n%ld check groups, %d failures\n", g_checks, g_fail);
    if (g_fail != 0) {
        printf("RESULT: FAIL (%d failures)\n", g_fail);
        return 1;
    }
    printf("RESULT: PASS\n");
    return 0;
}

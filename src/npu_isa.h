/* =============================================================================
 * npu_isa.h - Public API for NPU custom-ISA style hotspot operators.
 *
 * Every operator is defined against a bit-exact C reference (see npu_ref.h).
 * Optimized backends (AVX2 intrinsics and hand-written assembly) MUST produce
 * results identical to that reference for every input, including saturation
 * edge cases.
 * ========================================================================== */
#ifndef NPU_ISA_H
#define NPU_ISA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* vmac.sat.i16 : out[i] = sat_i16( (int32)c[i] + (int32)a[i] * (int32)b[i] ) */
void npu_vmac_sat_i16(const int16_t *a, const int16_t *b,
                      const int16_t *c, int16_t *out, size_t n);

/* vadd.sat.i16 : out[i] = sat_i16( (int32)a[i] + (int32)b[i] ) */
void npu_vadd_sat_i16(const int16_t *a, const int16_t *b,
                      int16_t *out, size_t n);

/* Returns non-zero if the CPU supports AVX2 (0 on non-x86). */
int npu_hw_has_avx2(void);

#if defined(__x86_64__) || defined(__i386__)
void npu_vmac_sat_i16_avx2_intrin(const int16_t *a, const int16_t *b,
                                  const int16_t *c, int16_t *out, size_t n);
void npu_vadd_sat_i16_avx2_intrin(const int16_t *a, const int16_t *b,
                                  int16_t *out, size_t n);
#ifdef NPU_USE_ASM_KERNELS
void npu_vmac_sat_i16_avx2(const int16_t *a, const int16_t *b,
                           const int16_t *c, int16_t *out, size_t n);
void npu_vadd_sat_i16_avx2(const int16_t *a, const int16_t *b,
                           int16_t *out, size_t n);
#endif
#endif

#ifdef __cplusplus
}
#endif
#endif

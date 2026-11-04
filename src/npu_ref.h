/* =============================================================================
 * npu_ref.h - Bit-exact C reference for the NPU hotspot operators.
 * This header is the SPECIFICATION / oracle used for verification.
 * ========================================================================== */
#ifndef NPU_REF_H
#define NPU_REF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Clamp a 32-bit value into the signed 16-bit range (saturating ALU). */
static inline int16_t npu_sat_i16(int32_t v)
{
    if (v > 32767)  return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

int16_t npu_ref_vmac_sat_i16(int16_t a, int16_t b, int16_t c);
int16_t npu_ref_vadd_sat_i16(int16_t a, int16_t b);
void npu_ref_vmac_sat_i16_vec(const int16_t *a, const int16_t *b,
                              const int16_t *c, int16_t *out, size_t n);
void npu_ref_vadd_sat_i16_vec(const int16_t *a, const int16_t *b,
                              int16_t *out, size_t n);

#ifdef __cplusplus
}
#endif
#endif

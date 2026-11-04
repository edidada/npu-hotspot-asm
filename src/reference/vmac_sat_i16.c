/* Reference implementation of vmac.sat.i16 (the verification oracle). */
#include "npu_ref.h"

int16_t npu_ref_vmac_sat_i16(int16_t a, int16_t b, int16_t c)
{
    int32_t p = (int32_t)a * (int32_t)b;
    int32_t r = (int32_t)c + p;
    return npu_sat_i16(r);
}

void npu_ref_vmac_sat_i16_vec(const int16_t *a, const int16_t *b,
                              const int16_t *c, int16_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++)
        out[i] = npu_ref_vmac_sat_i16(a[i], b[i], c[i]);
}

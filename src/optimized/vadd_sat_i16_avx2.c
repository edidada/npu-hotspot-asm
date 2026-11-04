/* vadd.sat.i16 - AVX2 implementation via compiler intrinsics.
 * _mm256_adds_epi16 performs signed saturating addition directly. */
#include "npu_isa.h"
#include "npu_ref.h"
#include <immintrin.h>

void npu_vadd_sat_i16_avx2_intrin(const int16_t *a, const int16_t *b,
                                  int16_t *out, size_t n)
{
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        __m256i va = _mm256_loadu_si256((const __m256i *)(a + i));
        __m256i vb = _mm256_loadu_si256((const __m256i *)(b + i));
        __m256i o = _mm256_adds_epi16(va, vb);
        _mm256_storeu_si256((__m256i *)(out + i), o);
    }
    for (; i < n; i++)
        out[i] = npu_ref_vadd_sat_i16(a[i], b[i]);
}

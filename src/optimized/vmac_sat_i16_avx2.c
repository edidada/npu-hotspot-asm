/* vmac.sat.i16 - AVX2 implementation via compiler intrinsics.
 *
 * This is the "intrinsics first" stage from the README: more maintainable
 * than raw assembly, still SIMD-vectorized. It is kept in the tree as a
 * verification target and as the optimized backend on platforms where the
 * hand-written assembly kernel is not built (non-Linux x86).
 *
 * Pipeline (16 int16 per iteration, 8 int32 per 128-bit lane):
 *   1. Sign-extend a/b/c halves from int16 to int32.
 *   2. 32-bit multiply, 32-bit add with c.
 *   3. packssdw saturates int32 -> int16 (per-lane).
 *   4. permute 0xD8 reorders the 64-bit lanes back to natural index order.
 */
#include "npu_isa.h"
#include "npu_ref.h"
#include <immintrin.h>

void npu_vmac_sat_i16_avx2_intrin(const int16_t *a, const int16_t *b,
                                  const int16_t *c, int16_t *out, size_t n)
{
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        __m256i va = _mm256_loadu_si256((const __m256i *)(a + i));
        __m256i vb = _mm256_loadu_si256((const __m256i *)(b + i));
        __m256i vc = _mm256_loadu_si256((const __m256i *)(c + i));

        __m256i aL = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(va));
        __m256i aH = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(va, 1));
        __m256i bL = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(vb));
        __m256i bH = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(vb, 1));
        __m256i cL = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(vc));
        __m256i cH = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(vc, 1));

        __m256i pL = _mm256_mullo_epi32(aL, bL);
        __m256i pH = _mm256_mullo_epi32(aH, bH);
        __m256i rL = _mm256_add_epi32(pL, cL);
        __m256i rH = _mm256_add_epi32(pH, cH);

        __m256i o = _mm256_packs_epi32(rL, rH);
        o = _mm256_permute4x64_epi64(o, 0xD8);
        _mm256_storeu_si256((__m256i *)(out + i), o);
    }
    /* Scalar tail (also handles n < 16). Must match the reference exactly. */
    for (; i < n; i++)
        out[i] = npu_ref_vmac_sat_i16(a[i], b[i], c[i]);
}

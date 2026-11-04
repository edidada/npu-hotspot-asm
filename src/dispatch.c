/* =============================================================================
 * dispatch.c - runtime backend selection for the NPU hotspot operators.
 *
 * Strategy (see README: "intrinsics first, assembly last mile"):
 *   - On x86 with AVX2, use the hand-written assembly kernel (Linux/ELF) or
 *     the AVX2 intrinsics kernel (other x86).
 *   - Otherwise fall back to the bit-exact scalar reference.
 *
 * The fallback makes the library safe and correct on any platform, while the
 * optimized path is only taken when the hardware is known to support it.
 * ========================================================================== */
#include "npu_isa.h"
#include "npu_ref.h"

#if defined(__x86_64__) || defined(__i386__)
#  if defined(_MSC_VER)
#    include <intrin.h>
     static int cpuid_has_avx2(void)
     {
         int regs[4];
         __cpuidex(regs, 7, 0);
         return (regs[1] & (1 << 5)) != 0;   /* AVX2 = EBX bit 5 */
     }
#  elif defined(__GNUC__) || defined(__clang__)
#    include <cpuid.h>
     static int cpuid_has_avx2(void)
     {
         unsigned int eax, ebx, ecx, edx;
         if (__get_cpuid_max(0, NULL) < 7) return 0;
         __cpuid_count(7, 0, eax, ebx, ecx, edx);
         return (ebx & (1u << 5)) != 0;
     }
#  else
     static int cpuid_has_avx2(void) { return 0; }
#  endif
#else
     static int cpuid_has_avx2(void) { return 0; }
#endif

int npu_hw_has_avx2(void) { return cpuid_has_avx2(); }

void npu_vmac_sat_i16(const int16_t *a, const int16_t *b,
                      const int16_t *c, int16_t *out, size_t n)
{
#if defined(__x86_64__) || defined(__i386__)
#  if defined(NPU_USE_ASM_KERNELS)
    if (cpuid_has_avx2()) { npu_vmac_sat_i16_avx2(a, b, c, out, n); return; }
#  else
    if (cpuid_has_avx2()) { npu_vmac_sat_i16_avx2_intrin(a, b, c, out, n); return; }
#  endif
#endif
    npu_ref_vmac_sat_i16_vec(a, b, c, out, n);
}

void npu_vadd_sat_i16(const int16_t *a, const int16_t *b, int16_t *out, size_t n)
{
#if defined(__x86_64__) || defined(__i386__)
#  if defined(NPU_USE_ASM_KERNELS)
    if (cpuid_has_avx2()) { npu_vadd_sat_i16_avx2(a, b, out, n); return; }
#  else
    if (cpuid_has_avx2()) { npu_vadd_sat_i16_avx2_intrin(a, b, out, n); return; }
#  endif
#endif
    npu_ref_vadd_sat_i16_vec(a, b, out, n);
}

/* =============================================================================
 * vmac.sat.i16 - hand-written AVX2 kernel (System V AMD64 ABI).
 *
 *   out[i] = sat_i16( (int32)c[i] + (int32)a[i] * (int32)b[i] )
 *
 * ABI: a=rdi, b=rsi, c=rdx, out=rcx, n=r8.
 * All YMM registers are volatile under the System V ABI, so none are saved.
 *
 * Per 16-element iteration:
 *   - sign-extend each int16 half to int32 (vpmovsxwd)
 *   - full 32-bit multiply + 32-bit add (vpmulld / vpaddd)
 *   - vpackssdw saturates int32 -> int16, per 128-bit lane
 *   - vpermq 0xD8 reorders 64-bit lanes to natural index order
 * ========================================================================== */
        .intel_syntax noprefix
        .text

        .globl  npu_vmac_sat_i16_avx2
#if defined(__ELF__)
        .type   npu_vmac_sat_i16_avx2, @function
#endif
npu_vmac_sat_i16_avx2:
        test    r8, r8
        jz      .Lvmac_done
.Lvmac_loop:
        cmp     r8, 16
        jl      .Lvmac_done

        vmovdqu ymm0, [rdi]            /* a */
        vmovdqu ymm1, [rsi]            /* b */
        vmovdqu ymm2, [rdx]            /* c */

        vpmovsxwd ymm3, xmm0           /* a[0..7]  -> int32 */
        vextracti128 xmm14, ymm0, 1
        vpmovsxwd ymm4, xmm14          /* a[8..15] -> int32 */

        vpmovsxwd ymm5, xmm1           /* b[0..7]  -> int32 */
        vextracti128 xmm14, ymm1, 1
        vpmovsxwd ymm6, xmm14          /* b[8..15] -> int32 */

        vpmovsxwd ymm7, xmm2           /* c[0..7]  -> int32 */
        vextracti128 xmm14, ymm2, 1
        vpmovsxwd ymm8, xmm14          /* c[8..15] -> int32 */

        vpmulld ymm9,  ymm3, ymm5      /* aL*bL */
        vpmulld ymm10, ymm4, ymm6      /* aH*bH */
        vpaddd  ymm11, ymm9, ymm7      /* + cL  */
        vpaddd  ymm12, ymm10, ymm8     /* + cH  */

        vpackssdw ymm13, ymm11, ymm12  /* sat int32 -> int16 (per lane) */
        vpermq ymm13, ymm13, 0xD8      /* reorder 64-bit lanes -> index order */
        vmovdqu [rcx], ymm13

        add     rdi, 32
        add     rsi, 32
        add     rdx, 32
        add     rcx, 32
        sub     r8, 16
        jmp     .Lvmac_loop

.Lvmac_done:
        vzeroupper
        ret
#if defined(__ELF__)
        .size   npu_vmac_sat_i16_avx2, .-npu_vmac_sat_i16_avx2
#endif

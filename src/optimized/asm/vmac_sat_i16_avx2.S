/* =============================================================================
 * vmac.sat.i16 - hand-written AVX2 kernel (System V AMD64 ABI).
 *   out[i] = sat_i16( (int32)c[i] + (int32)a[i] * (int32)b[i] )
 * ABI: a=rdi, b=rsi, c=rdx, out=rcx, n=r8.
 * A scalar tail processes the final <16 elements (bit-exact to the reference).
 * ========================================================================== */
        .intel_syntax noprefix
        .text

        .globl  npu_vmac_sat_i16_avx2
#if defined(__ELF__)
        .type   npu_vmac_sat_i16_avx2, @function
#endif
npu_vmac_sat_i16_avx2:
        test    r8, r8
        jz      .Lvmac_ret
.Lvmac_loop:
        cmp     r8, 16
        jl      .Lvmac_tail

        vmovdqu ymm0, [rdi]
        vmovdqu ymm1, [rsi]
        vmovdqu ymm2, [rdx]

        vpmovsxwd ymm3, xmm0
        vextracti128 xmm14, ymm0, 1
        vpmovsxwd ymm4, xmm14

        vpmovsxwd ymm5, xmm1
        vextracti128 xmm14, ymm1, 1
        vpmovsxwd ymm6, xmm14

        vpmovsxwd ymm7, xmm2
        vextracti128 xmm14, ymm2, 1
        vpmovsxwd ymm8, xmm14

        vpmulld ymm9,  ymm3, ymm5
        vpmulld ymm10, ymm4, ymm6
        vpaddd  ymm11, ymm9, ymm7
        vpaddd  ymm12, ymm10, ymm8

        vpackssdw ymm13, ymm11, ymm12
        vpermq ymm13, ymm13, 0xD8
        vmovdqu [rcx], ymm13

        add     rdi, 32
        add     rsi, 32
        add     rdx, 32
        add     rcx, 32
        sub     r8, 16
        jmp     .Lvmac_loop

.Lvmac_tail:
        xor     r9, r9
.Lvmac_tloop:
        cmp     r9, r8
        jge     .Lvmac_ret
        movsx   r10, word ptr [rdi + r9*2]
        movsx   r11, word ptr [rsi + r9*2]
        movsx   rax, word ptr [rdx + r9*2]
        imul    r10, r11
        add     r10, rax
        mov     r11, 32767
        cmp     r10, r11
        cmovg   r10, r11
        mov     r11, -32768
        cmp     r10, r11
        cmovl   r10, r11
        mov     word ptr [rcx + r9*2], r10w
        inc     r9
        jmp     .Lvmac_tloop

.Lvmac_ret:
        vzeroupper
        ret
#if defined(__ELF__)
        .size   npu_vmac_sat_i16_avx2, .-npu_vmac_sat_i16_avx2
#endif

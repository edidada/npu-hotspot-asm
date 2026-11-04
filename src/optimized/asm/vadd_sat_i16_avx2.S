/* =============================================================================
 * vadd.sat.i16 - hand-written AVX2 kernel (System V AMD64 ABI).
 *   out[i] = sat_i16( (int32)a[i] + (int32)b[i] )
 * ABI: a=rdi, b=rsi, out=rcx, n=rdx.
 * A scalar tail processes the final <16 elements (bit-exact to the reference).
 * ========================================================================== */
        .intel_syntax noprefix
        .text

        .globl  npu_vadd_sat_i16_avx2
#if defined(__ELF__)
        .type   npu_vadd_sat_i16_avx2, @function
#endif
npu_vadd_sat_i16_avx2:
        test    rdx, rdx
        jz      .Lvadd_ret
.Lvadd_loop:
        cmp     rdx, 16
        jl      .Lvadd_tail

        vmovdqu ymm0, [rdi]
        vmovdqu ymm1, [rsi]
        vpaddsw ymm2, ymm0, ymm1
        vmovdqu [rcx], ymm2

        add     rdi, 32
        add     rsi, 32
        add     rcx, 32
        sub     rdx, 16
        jmp     .Lvadd_loop

.Lvadd_tail:
        xor     r9, r9
.Lvadd_tloop:
        cmp     r9, rdx
        jge     .Lvadd_ret
        movsx   r10, word ptr [rdi + r9*2]
        movsx   r11, word ptr [rsi + r9*2]
        add     r10, r11
        mov     r11, 32767
        cmp     r10, r11
        cmovg   r10, r11
        mov     r11, -32768
        cmp     r10, r11
        cmovl   r10, r11
        mov     word ptr [rcx + r9*2], r10w
        inc     r9
        jmp     .Lvadd_tloop

.Lvadd_ret:
        vzeroupper
        ret
#if defined(__ELF__)
        .size   npu_vadd_sat_i16_avx2, .-npu_vadd_sat_i16_avx2
#endif
        .section .note.GNU-stack,"",@progbits

/* =============================================================================
 * vadd.sat.i16 - hand-written AVX2 kernel (System V AMD64 ABI).
 *
 *   out[i] = sat_i16( (int32)a[i] + (int32)b[i] )
 *
 * ABI: a=rdi, b=rsi, out=rcx, n=rdx.
 * vpaddsw performs signed saturating addition on packed int16 directly.
 * ========================================================================== */
        .intel_syntax noprefix
        .text

        .globl  npu_vadd_sat_i16_avx2
#if defined(__ELF__)
        .type   npu_vadd_sat_i16_avx2, @function
#endif
npu_vadd_sat_i16_avx2:
        test    rdx, rdx
        jz      .Lvadd_done
.Lvadd_loop:
        cmp     rdx, 16
        jl      .Lvadd_done

        vmovdqu ymm0, [rdi]
        vmovdqu ymm1, [rsi]
        vpaddsw ymm2, ymm0, ymm1
        vmovdqu [rcx], ymm2

        add     rdi, 32
        add     rsi, 32
        add     rcx, 32
        sub     rdx, 16
        jmp     .Lvadd_loop

.Lvadd_done:
        vzeroupper
        ret
#if defined(__ELF__)
        .size   npu_vadd_sat_i16_avx2, .-npu_vadd_sat_i16_avx2
#endif

# Design

This project is a working, minimal embodiment of the engineering practice
outlined in the top-level README: when an NPU custom-ISA operator is a proven
hotspot and the compiler cannot reach the target, hand-tuned assembly is the
*last-mile* tool — applied to the smallest possible fragment, surrounded by a
bit-exact reference and an automatic verification + performance gate.

## Operators

Two NPU-style SIMD/ALU operators are modelled. Both are defined purely by
their C reference semantics, which is the specification everything else is
checked against.

| Operator        | Semantics                                                   | Notes |
|-----------------|-------------------------------------------------------------|-------|
| `vmac.sat.i16`  | `out[i] = sat_i16(c[i] + a[i]*b[i])` (int32 internal)       | classic NPU MAC with saturation |
| `vadd.sat.i16`  | `out[i] = sat_i16(a[i] + b[i])` (int32 internal)           | saturating add |

`out`, `a`, `b`, `c` are `int16_t` arrays of length `n`. `sat_i16` clamps to
the signed 16-bit range `[-32768, 32767]`.

## Backend layers

```
        application code
              |
              v
        npu_vmac_sat_i16 / npu_vadd_sat_i16   <- dispatch entry points
              |
   +----------+----------+
   |                     |
   v                     v
 reference oracle   optimized backend (x86/AVX2)
 (scalar, always      - intrinsics (portable x86)
  correct, fallback)  - hand-written asm (Linux/System V, production hot path)
```

* **Reference oracle** (`src/reference/`) — trivially correct scalar code. It
  is the verification target and the portable fallback.
* **Intrinsics backend** (`src/optimized/*_avx2.c`) — the "intrinsics first"
  step. Easier to maintain than raw assembly, still SIMD-vectorized. Used as
  the optimized path on non-Linux x86 and as a second verification target.
* **Hand-written assembly** (`src/optimized/asm/*.S`) — the last-mile kernel,
  handwritten for precise control of scheduling and register use. Built and
  used on Linux x86-64 (System V ABI, where all YMM registers are volatile).

## Why these two implementations exist side by side

The README argues for *intrinsics first, assembly last*. Keeping both lets the
CI prove the assembly is bit-exact against (a) the reference and (b) the
intrinsics version. If the assembly ever drifts, the verification harness
flags it before it can ship.

## Data layout & vectorization notes (`vmac.sat.i16`)

Per 16-element (256-bit `ymm`) iteration:

1. Sign-extend each `int16` half to `int32` with `vpmovsxwd` (low 128-bit and
   extracted high 128-bit), giving eight `int32` per lane.
2. Full 32-bit multiply (`vpmulld`) then 32-bit add with `c` (`vpaddd`) — no
   intermediate saturation, so the result is mathematically exact in `int32`.
3. `vpackssdw` saturates `int32 -> int16` per 128-bit lane. Because packing
   interleaves lanes, a `vpermq` with immediate `0xD8` reorders the 64-bit
   lanes back into natural index order before the store.
4. A scalar tail (and `n < 16`) is handled by the reference oracle so the
   kernel stays aligned and simple.

`vadd.sat.i16` is simpler: `vpaddsw` performs signed saturating addition
natively on packed `int16`.

## Runtime dispatch

`src/dispatch.c` selects a backend with CPUID (AVX2 = leaf 7, EBX bit 5):

* AVX2 present and `NPU_USE_ASM_KERNELS` defined (Linux build) -> hand-written asm.
* AVX2 present, otherwise -> AVX2 intrinsics.
* No AVX2 -> scalar reference.

The fallback guarantees the library is correct and never SIGILLs on hardware
without AVX2.

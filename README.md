# npu-hotspot-asm

Industrial-grade template for hand-optimized assembly on **NPU custom-ISA
hotspot operators**: a bit-exact C reference, an AVX2 intrinsics backend, a
hand-written AVX2 assembly kernel, runtime dispatch, an automatic bit-exact
verification harness, and a performance gate — all wired into GitHub Actions.

This repository is a working embodiment of the analysis below: hand-tuned
assembly is treated as a *last-mile* tool, bounded by a reference oracle and
continuous verification, never as a free-standing source of truth.

## Background: is hand-written assembly worth it for NPU custom ISA operators?

> In NPU custom-instruction operators, hand-written optimized assembly (or
> hand-written intrinsics / inline assembly) is reasonable and common, but it
> is usually *not* the first choice — it is the last-mile tool used when the
> compiler-generated code cannot hit the target. It is appropriate when
> several conditions hold at once:
>
> 1. **The compiler models the custom instruction incompletely** — new
>    instructions, special data layouts, non-standard accumulate/saturation
>    semantics that the backend does not cover or schedules poorly.
> 2. **The bottleneck is clear and attributable** — profiling shows the hotspot
>    is a specific kernel, and the gap comes from instruction selection,
>    register allocation, or scheduling — not memory or algorithm.
> 3. **The gain is quantifiable** — the hand-written version shows a measurable
>    speedup over the compiler version (commonly 10%–2x, higher for
>    layout-sensitive operators), with acceptable maintenance cost.
> 4. **The scenario is stable** — the operator / ISA version is relatively
>    fixed, so the hand-written code does not go stale quickly.
>
> NPU custom ISAs especially invite hand-writing because instruction semantics
> are non-standard (matmul-accumulate, vector saturation, lookup, bit-packing,
> sparse skip), data layout is sensitive, double-buffering/DMA overlap needs
> precise pipeline control, on-chip storage is scarce, and instruction
> latency/throughput is often undocumented so the compiler scheduler is
> conservative.
>
> The costs and risks are real: portability drops to zero, correctness risk is
> high (saturation, rounding, boundaries — needs bit-exact testing against a
> reference), it can drift from compiler improvements, and debugging needs
> simulators / waveforms / instruction traces.
>
> The pragmatic engineering path (recommended order): (1) express the operator
> in a high-level DSL/compiler + custom intrinsics and inspect codegen; (2)
> rewrite the hotspot kernel with intrinsics, wrapping only key instructions in
> `asm volatile`; (3) only if still short, hand-write assembly for the smallest
> hotspot fragment, keeping a C reference for bit-exact validation; (4) build
> automatic regression + performance gates so ISA/compiler upgrades cannot
> silently degrade the hand-written code.
>
> **One-line summary:** hand-written assembly is a legitimate, scenario-bound
> tool for NPU custom-ISA operators; trigger conditions are "compiler coverage
> gap + clear hotspot + quantifiable gain + stable scenario"; prefer intrinsics,
> confine raw assembly to the smallest hotspot, and control risk with bit-exact
> comparison and a performance gate.

## What this repo implements

Two NPU-style operators, each with three backends:

| Operator        | Semantics                                        | Backends |
|-----------------|--------------------------------------------------|----------|
| `vmac.sat.i16`  | `out[i] = sat_i16(c[i] + a[i]*b[i])` (int32)     | ref / intrinsics / asm |
| `vadd.sat.i16`  | `out[i] = sat_i16(a[i] + b[i])` (int32)          | ref / intrinsics / asm |

* `src/reference/` — bit-exact scalar **oracle** (specification + fallback).
* `src/optimized/*_avx2.c` — AVX2 **intrinsics** backend.
* `src/optimized/asm/*.S` — hand-written **AVX2 assembly** kernel (Linux/System V).
* `src/dispatch.c` — runtime backend selection via CPUID (AVX2 = leaf 7, EBX bit 5).
* `tests/test_main.c` — bit-exact verification (edge + fuzz + tail).
* `bench/bench_main.c` — throughput benchmark + performance gate.

## Build & run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Bit-exact verification (must pass)
ctest --test-dir build --output-on-failure

# Performance gate (speedup vs scalar oracle)
./build/npu_bench
```

On Linux x86-64 the hand-written assembly kernel is built and used as the
production hot path. On other x86 the AVX2 intrinsics backend is used; without
AVX2 the scalar reference is used (safe, correct, never SIGILLs).

## CI

`.github/workflows/ci.yml` builds with gcc and clang on `ubuntu-latest`, runs
`ctest` (bit-exact verification) and `npu_bench` (performance gate) on every
push/PR. The performance gate floor is relaxed to `1.5x` on CI to tolerate
shared-runner noise; the optimized path should clear it by a wide margin.

## Documentation

* [docs/design.md](docs/design.md) — operators, backend layering, vectorization notes.
* [docs/verification.md](docs/verification.md) — bit-exact verification & performance gating.

## License

MIT — see [LICENSE](LICENSE).

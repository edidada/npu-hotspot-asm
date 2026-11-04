# Verification & Performance Gating

Hand-written assembly around custom instruction semantics is high-risk: a
wrong saturation, rounding, or lane permutation is invisible until a specific
input hits it. This project controls that risk with two automated gates that
run in CI on every push/PR.

## 1. Bit-exact verification (`tests/test_main.c`)

The reference oracle in `src/reference/` is the source of truth. The harness
compares each optimized backend against it:

* **Edge cases** — explicit saturation/overflow inputs:
  * `32767 * 32767 + 0` -> `32767`
  * `(-32768) * (-32768) + 0` -> `32767`
  * `-300 * 200 + 0` -> `-32768`
  * `200 * 200 + 1000` -> `32767`
  * `32767 + 1` -> `32767`, `-32768 + (-1)` -> `-32768`
* **Randomized fuzzing** — for lengths
  `{0, 1, 2, 15, 16, 17, 31, 32, 33, 100, 1000, 50000}` and 8 random reps each,
  full-range `int16` inputs. Lengths that are not multiples of 16 exercise the
  scalar tail path and its interaction with the vectorized body.

Backends checked: the dispatch entry point (production path), the AVX2
intrinsics variant, and — when built — the hand-written assembly variant.

A single mismatched element fails the build. There is no tolerance for
drift: the whole point is that optimized == reference, bit for bit.

Run locally:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 2. Performance gate (`bench/bench_main.c`)

The optimized backend is benchmarked against the reference oracle over 1M
elements (30 iterations, `CLOCK_MONOTONIC`). The reference is compiled with
auto-vectorization disabled (`-fno-tree-vectorize`) so the comparison is a
fair scalar baseline rather than the compiler also emitting SIMD.

Policy:

* If AVX2 is unavailable, the optimized path *is* the reference, so the gate
  is skipped (nothing meaningful to compare).
* Otherwise a speedup below `NPU_MIN_SPEEDUP` (default `2.0`, overridden in CI
  to `1.5` for shared-runner noise tolerance) is treated as a regression.

The gate catches the failure mode where an "optimization" is actually slower
(e.g. a bad schedule, redundant moves, or a fallback being taken by mistake).

Run locally:

```sh
./build/npu_bench
NPU_MIN_SPEEDUP=3.0 ./build/npu_bench   # stricter local check
```

## Why both gates

* Bit-exact verification answers **"is it correct?"**.
* The performance gate answers **"is it worth the maintenance cost?"** — the
  question the README says should be asked before reaching for assembly.

Together they make hand-tuned assembly a safe, auditable engineering choice
rather than a fragile one.

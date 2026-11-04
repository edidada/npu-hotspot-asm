# =============================================================================
# npu-hotspot-asm - CMake build
#
# Layout:
#   - npu_hotspot        : dispatch + bit-exact scalar reference (always built)
#   - npu_hotspot_avx2_intrin : AVX2 intrinsics backends (x86)
#   - npu_hotspot_asm    : hand-written AVX2 assembly backends (Linux/x86_64)
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(npu_hotspot_asm C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(NPU_BUILD_TESTS "Build and register the verification tests" ON)
option(NPU_BUILD_BENCH "Build the performance benchmark" ON)

# --- Core library: dispatch + reference oracle ------------------------------
add_library(npu_hotspot STATIC
    src/reference/vmac_sat_i16.c
    src/reference/vadd_sat_i16.c
    src/dispatch.c)
target_include_directories(npu_hotspot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# The reference oracle must stay a genuine scalar baseline, so disable the
# compiler's own vectorization of it; this keeps the perf gate meaningful.
target_compile_options(npu_hotspot PRIVATE -O2 -Wall -Wextra -fno-tree-vectorize)

# --- x86 optimized backends -------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86|X86|amd|AMD|Intel|intel)")
    message(STATUS "NPU: x86 target detected (${CMAKE_SYSTEM_PROCESSOR})")

    add_library(npu_hotspot_avx2_intrin STATIC
        src/optimized/vmac_sat_i16_avx2.c
        src/optimized/vadd_sat_i16_avx2.c)
    target_include_directories(npu_hotspot_avx2_intrin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(npu_hotspot_avx2_intrin PRIVATE -mavx2 -O3 -Wall -Wextra)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(STATUS "NPU: building hand-written AVX2 assembly kernels (Linux/System V)")
        set_source_files_properties(
            src/optimized/asm/vmac_sat_i16_avx2.S
            src/optimized/asm/vadd_sat_i16_avx2.S
            PROPERTIES LANGUAGE ASM)
        add_library(npu_hotspot_asm STATIC
            src/optimized/asm/vmac_sat_i16_avx2.S
            src/optimized/asm/vadd_sat_i16_avx2.S)
        target_include_directories(npu_hotspot_asm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
        # NPU_USE_ASM_KERNELS tells dispatch.c to call the assembly kernels.
        target_compile_definitions(npu_hotspot_asm PUBLIC NPU_USE_ASM_KERNELS)
        target_link_libraries(npu_hotspot PUBLIC npu_hotspot_asm)
    else()
        message(STATUS "NPU: non-Linux x86 -> using AVX2 intrinsics as optimized path")
        target_link_libraries(npu_hotspot PUBLIC npu_hotspot_avx2_intrin)
    endif()
endif()

# --- Tests ------------------------------------------------------------------
if(NPU_BUILD_TESTS)
    enable_testing()
    add_executable(npu_tests tests/test_main.c)
    target_include_directories(npu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(npu_tests PRIVATE npu_hotspot npu_hotspot_avx2_intrin)
    add_test(NAME npu_bit_exact COMMAND npu_tests)
endif()

# --- Benchmark --------------------------------------------------------------
if(NPU_BUILD_BENCH)
    add_executable(npu_bench bench/bench_main.c)
    target_include_directories(npu_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(npu_bench PRIVATE npu_hotspot)
endif()

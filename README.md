# RVForge

RISC-V cross-compilation harness for high-precision numerical libraries.

[![CI](https://github.com/codewithayuu/RVForge/actions/workflows/riscv64.yml/badge.svg)](https://github.com/codewithayuu/RVForge/actions/workflows/riscv64.yml)

---

## What This Is

RVForge cross-compiles two production numerical libraries — **MPFR 4.2.1**
and **OpenBLAS 0.3.26** — to `riscv64-linux-gnu`, validates their numerical
output against x86_64 native results under `qemu-riscv64-static`, and ships
a CI pipeline that prevents future bit-rot.

Every x86 SIMD intrinsic that appears in the RISC-V build path is replaced
with a scalar fallback gated behind `#if defined(__riscv)`. Five routines
are additionally rewritten using RVV intrinsics (`<riscv_vector.h>`).

---

## Reproduce in One Command

```bash
# Full build + validation (Ubuntu 22.04)
sudo apt-get install -y \
    gcc-riscv64-linux-gnu g++-riscv64-linux-gnu \
    binutils-riscv64-linux-gnu qemu-user-static \
    cmake ninja-build autoconf automake libtool \
    texinfo m4 wget xz-utils python3

./scripts/bootstrap.sh --install-deps

cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv64-linux-gnu.cmake \
    -DRVFORGE_BUILD_MPFR=ON \
    -DRVFORGE_BUILD_OPENBLAS=ON \
    -DRVFORGE_ENABLE_RVV=OFF \
    -DRVFORGE_RUN_TESTS=ON \
    -G Ninja

cmake --build build -- -j$(nproc)

EPSILON=1e-10 ./scripts/validate_qemu.sh
```

---

## Libraries

| Library | Version | Why chosen |
|---------|---------|-----------|
| **MPFR** | 4.2.1 | Pure C, correct multi-precision semantics, rigorous test suite, no SIMD in core |
| **OpenBLAS** | 0.3.26 | Has extensive x86 SIMD kernel directory, clear `ARCH`/`TARGET` fallback mechanism, real numerical workloads |

---

## Toolchain File Parameters

`toolchain/riscv64-linux-gnu.cmake` 

| Parameter | Value | Reason |
|-----------|-------|--------|
| `CMAKE_SYSTEM_NAME` | `Linux` | Not bare-metal; enables Linux ABI assumptions |
| `CMAKE_SYSTEM_PROCESSOR` | `riscv64` | Enables `if(CMAKE_SYSTEM_PROCESSOR MATCHES riscv)` guards |
| `CMAKE_C_COMPILER` | `riscv64-linux-gnu-gcc` | Debian cross-toolchain binary |
| `CMAKE_AR` / `RANLIB` | `riscv64-linux-gnu-ar/ranlib` | Static library tools must match target |
| `CMAKE_FIND_ROOT_PATH` | `/usr/riscv64-linux-gnu` | RISC-V sysroot location |
| `CMAKE_FIND_ROOT_PATH_MODE_PROGRAM` | `NEVER` | Host tools (cmake, make) not in sysroot |
| `CMAKE_FIND_ROOT_PATH_MODE_LIBRARY` | `ONLY` | All libs from RISC-V sysroot only |
| `CMAKE_FIND_ROOT_PATH_MODE_INCLUDE` | `ONLY` | All headers from RISC-V sysroot only |
| `CMAKE_C_FLAGS_INIT` | `-march=rv64gc -mabi=lp64d` | rv64gc = base+compressed+D float; lp64d = 64-bit pointers, FP args in FP regs |
| `CMAKE_EXE_LINKER_FLAGS_INIT` | `-static -static-libgcc` | Static binaries run under QEMU user-mode without host lib path issues |
| `CMAKE_CROSSCOMPILING_EMULATOR` | `qemu-riscv64-static -L ...` | Allows CMake to execute cross-compiled test programs at configure time |
| `CMAKE_TRY_COMPILE_TARGET_TYPE` | `STATIC_LIBRARY` | Prevents CMake from trying to link+run compiler sanity check |

---

## SIMD Intrinsic Audit

All x86 SIMD intrinsics found in the OpenBLAS source files that intersect
the `ARCH=riscv64` build path, and their scalar replacements:

| Intrinsic | Category | Found in | Scalar Replacement | Patch |
|-----------|----------|----------|--------------------|-------|
| `_mm_load_pd` | SSE2 | `dgemv_t_4_SSE.c` | `memcpy` 16B | 0004 |
| `_mm_loadu_pd` | SSE2 | `dgemv_t_4_SSE.c` | `memcpy` 16B | 0004 |
| `_mm_store_pd` | SSE2 | `dgemv_t_4_SSE.c` | `memcpy` 16B | 0004 |
| `_mm_mul_pd` | SSE2 | `dgemv_t_4_SSE.c` | `a * b` scalar | 0004 |
| `_mm_add_pd` | SSE2 | `dgemv_t_4_SSE.c` | `a + b` scalar | 0004 |
| `_mm_hadd_pd` | SSE3 | `dgemv_t_4_SSE.c` | `lo+hi` per pair | 0004 |
| `_mm256_fmadd_pd` | AVX2+FMA | `dgemm_kernel_4x8_haswell.c` | `fma(a,b,c)` | 0005 |
| `_mm256_fmsub_pd` | AVX2+FMA | `dgemm_kernel_4x8_haswell.c` | `fma(a,b,-c)` | 0005 |
| `_mm256_loadu_pd` | AVX | `dgemm_kernel_4x8_haswell.c` | `memcpy` 32B | 0005 |
| `_mm256_storeu_pd` | AVX | `dgemm_kernel_4x8_haswell.c` | `memcpy` 32B | 0005 |
| `_mm256_broadcast_sd` | AVX | `dgemm_kernel_4x8_haswell.c` | copy scalar | 0005 |
| `_mm256_blend_pd` | AVX | `dgemm_kernel_4x8_haswell.c` | `imm8` bit decode | 0005 |
| `_mm256_permute2f128_pd` | AVX | `dgemm_kernel_4x8_haswell.c` | index decode | 0005 |
| `_mm256_hadd_pd` | AVX | `dgemm_kernel_4x8_haswell.c` | pairwise scalar add | 0005 |
| `_mm256_fmadd_ps` | AVX2+FMA | `sgemm_kernel_16x4_sandy.c` | `fmaf(a,b,c)` | 0006 |
| `_mm256_loadu_ps` | AVX | `sgemm_kernel_16x4_sandy.c` | `memcpy` 32B | 0006 |
| `_mm256_store_ps` | AVX | `sgemm_kernel_16x4_sandy.c` | `memcpy` 32B | 0006 |
| `_mm256_broadcast_ss` | AVX | `sgemm_kernel_16x4_sandy.c` | copy scalar | 0006 |
| `_mm256_addsub_pd` | AVX | `zgemm_kernel_2x4_sandy.c` | `re=ac-bd; im=ad+bc` | 0007 |
| `_mm256_moveldup_pd` | AVX | `zgemm_kernel_2x4_sandy.c` | broadcast real lane | 0007 |
| `_mm256_movehdup_pd` | AVX | `zgemm_kernel_2x4_sandy.c` | broadcast imag lane | 0007 |
| `_mm_stream_pd` | SSE2 NT | `memory.c` | plain store | 0008 |
| `_mm_prefetch` | SSE | `interface/axpy.c` | `__builtin_prefetch` | 0008 |
| `_mm_malloc` | SSE | `memory.c` | `posix_memalign` | 0008 |
| `_mm_free` | SSE | `memory.c` | `free` | 0008 |
| `__cpuid` | x86 | `cpuid_x86.c` | `return 0` stub | 0003 |
| `__get_cpuid_count` | x86 | `cpuid_x86.c` | `return 0` stub | 0003 |

**All scalar fallbacks live in `simd_fallbacks/rvforge_simd.h`.**
Each is a `static inline` function with identical numerical semantics.
`_mm256_fmadd_pd` maps to `fma(a,b,c)` which the compiler lowers to
`FMADD.D` on rv64gc — no precision regression.

---

## Patches

| ID | File | What it fixes |
|----|------|--------------|
| `0001` | GMP `configure.ac` | Adds `riscv64-*-*` case to host detection; forces generic mpn path |
| `0002` | OpenBLAS `CMakeLists.txt` | Adds `RISCV64_GENERIC` to TARGET dispatch table |
| `0003` | OpenBLAS `cpuid_x86.c` | Wraps x86 CPUID in `#if !defined(__riscv)`; provides zero stubs |
| `0004` | OpenBLAS `dgemv_t_4_SSE.c` | Replaces SSE2 dgemv kernel with 4-way unrolled scalar |
| `0005` | OpenBLAS `dgemm_kernel_4x8_haswell.c` | Replaces AVX2+FMA dgemm with scalar `fma()` 4×8 block |
| `0006` | OpenBLAS `sgemm_kernel_16x4_sandy.c` | Replaces AVX sgemm with `fmaf()` 16×4 block |
| `0007` | OpenBLAS `zgemm_kernel_2x4_sandy.c` | Replaces AVX complex multiply with explicit re/im scalar |
| `0008` | OpenBLAS `memory.c`, `axpy.c` | Replaces non-temporal stores and prefetch with scalar equivalents |

Apply all patches:
```bash
./scripts/apply_patches.sh
```

---

## QEMU Validation Output

`mpfr_full_suite` under `qemu-riscv64-static`:

```
MPFR Full Validation Suite
MPFR version: 4.2.1
GMP  version: 6.3.0
Target: riscv64

--- Constants (multi-precision) ---
  pi( 64 bits) = 3.14159265358979323846264338327950...
  pi(256 bits) = 3.14159265358979323846264338327950288419716939937510...
  rounding_err = 0.000000e+00

--- Elementary Functions ---
  exp(0.00): err=0.000000e+00  OK
  exp(1.00): err=0.000000e+00  OK
  exp(2.00): err=0.000000e+00  OK
  ...

╔══════════════════════════════════════════════════════╗
║           MPFR Full Suite Summary                     ║
╠══════════════════════════════════════════════════════╣
║  const_pi                              ✅ PASS  ║
║  const_log2                            ✅ PASS  ║
║  exp_series                            ✅ PASS  ║
║  sqrt_newton                           ✅ PASS  ║
║  trig_identities                       ✅ PASS  ║
║  precision_levels                      ✅ PASS  ║
║  rounding_modes                        ✅ PASS  ║
║  special_values                        ✅ PASS  ║
╚══════════════════════════════════════════════════════╝

STATUS: PASS
```

`blas_full_suite` under `qemu-riscv64-static`:

```
BLAS Full Validation Suite
Target: riscv64

--- Level 1 BLAS ---
  DDOT: max_err=0.000e+00  PASS
  DAXPY (alpha=pi): max_err=0.000e+00  PASS
  DNRM2: max_err=2.842e-14  PASS
  DSCAL n=512: max_err=0.000e+00  PASS

--- Level 2 BLAS ---
  DGEMV N+T: max_err=2.274e-13  PASS
  DSYMV: max_err=1.137e-13  PASS
  DTRSV: max_err=8.527e-14  PASS

--- Level 3 BLAS ---
  DGEMM NN: max_err=1.137e-13  PASS
  DGEMM TT n=64: max_err=2.274e-13  PASS
  SGEMM n=128: max_err=4.768e-06  PASS
  DSYRK n=64 k=32: max_err=5.684e-14  PASS

STATUS: PASS
```

---

## x86_64 vs riscv64 Diff

| Test | x86_64 | riscv64 | max_err | Δ |
|------|--------|---------|---------|---|
| DDOT n=512 | `85.24795...` | `85.24795...` | `0.000e+00` | ✅ |
| DNRM2 n=512 | `2.631...` | `2.631...` | `2.842e-14` | ✅ |
| DGEMM 64×64 Frobenius | `147.329...` | `147.329...` | `1.137e-13` | ✅ |
| MPFR pi (256 bit) | `3.14159265358979...` | `3.14159265358979...` | `0.000e+00` | ✅ |
| MPFR exp(1) | `2.71828182845904...` | `2.71828182845904...` | `0.000e+00` | ✅ |

All comparisons within `epsilon=1e-10`. Full report: `docs/validation_report.md`.

---

## RVV Implementation

Five BLAS routines implemented with RVV 1.0 intrinsics (`-march=rv64gcv`):

| Routine | Key RVV intrinsics | LMUL | Description |
|---------|-------------------|------|-------------|
| `DDOT` | `vle64`, `vfmacc_vv`, `vfredusum` | m4 | FMA accumulation + reduction |
| `DAXPY` | `vle64`, `vfmacc_vf`, `vse64` | m8 | Broadcast scalar FMA |
| `DNRM2` | `vle64`, `vfmacc_vv`, `vfredusum` | m4 | Sum-of-squares + sqrt |
| `DGEMV` | `vle64`, `vfmacc_vv`, `vfredusum` | m4 | Row-wise dot + reduce |
| `DGEMM` | `vle64`, `vfmacc_vf`, `vse64` | m2 | Blocked panel kernel |

Run the benchmark:
```bash
./scripts/bench_rvv.sh
```

### Benchmark Numbers (qemu-riscv64-static)

| Kernel | N | Scalar (ns) | RVV (ns) | Speedup |
|--------|---|-------------|----------|---------|
| DDOT | 4096 | ~48000 | ~39000 | **1.23×** |
| DDOT | 16384 | ~192000 | ~148000 | **1.30×** |
| DAXPY | 1024 | ~9500 | ~8200 | **1.16×** |
| DAXPY | 16384 | ~152000 | ~118000 | **1.29×** |
| DNRM2 | 1024 | ~5800 | ~5100 | **1.14×** |
| DGEMV | 256×256 | ~600000 | ~460000 | **1.30×** |
| DGEMM | 64×64 | ~560000 | ~480000 | **1.17×** |

> QEMU emulates RVV at instruction level without vector hardware.
> Speedup reflects instruction-count reduction only.
> On real RISC-V silicon with VLEN≥256, expect **4–8×** for L1 BLAS.

Full analysis: `docs/rvv_analysis.md` 

---

## CI Pipeline

12 jobs — each has dedicated jobs with caching and artifact passing.

```
toolchain-probe
    └── fetch-and-patch
            ├── build-gmp
            │       └── build-mpfr ──────────────────┐
            ├── build-openblas ──────────────────────┤
            └── simd-audit                            │
                                                      ▼
                                              build-harness
                                              ├── qemu-validate
                                              ├── ctest-suite
                                              ├── upstream-tests
                                              └── rvv-correctness
bitrot-guard ────────────────────────────────────────┘
                                              └── final-summary
```

**Bit-rot guard** checks:
- SHA-256 hashes of toolchain file and all patches
- Patch count ≥ 8
- All `simd_fallbacks/` files present
- All `rvv/` files present
- Shell and Python script syntax

**Cache keys** include content hashes of CMakeLists and patch files —
a patch change invalidates only the affected library's cache entry.

---

## Project Layout

```
RVForge/
├── toolchain/
│   ├── riscv64-linux-gnu.cmake   CMake toolchain file
│   └── flags.cmake               Compile flag definitions
├── ports/
│   ├── mpfr/CMakeLists.txt       MPFR + GMP ExternalProject rules
│   └── openblas/CMakeLists.txt   OpenBLAS ExternalProject rules
├── patches/
│   ├── 0001..0008-*.patch        All patches in git-format-patch format
│   └── README.md                 Patch rationale + intrinsic mapping table
├── simd_fallbacks/
│   ├── rvforge_simd.h            50+ x86 intrinsic scalar stubs
│   ├── scalar_dgemv.c            SSE2 dgemv replacement
│   ├── scalar_dgemm.c            AVX2 dgemm replacement (blocked)
│   ├── scalar_sgemm.c            AVX sgemm replacement
│   └── scalar_memory.c           Memory/prefetch stubs
├── rvv/
│   ├── rvv_dispatch.h            Function declarations + RVFORGE_HAS_RVV
│   ├── rvv_ddot.c                RVV + scalar DDOT
│   ├── rvv_daxpy.c               RVV + scalar DAXPY
│   ├── rvv_dnrm2.c               RVV + scalar DNRM2
│   ├── rvv_dgemv.c               RVV + scalar DGEMV
│   └── rvv_dgemm.c               RVV + scalar DGEMM (blocked)
├── harness/
│   ├── mpfr_smoke.c              BBP pi + exp series
│   ├── blas_smoke.c              DDOT + DNRM2 + DGEMM basic
│   ├── mpfr_full_suite.c         8 MPFR test groups (50+ cases)
│   ├── blas_full_suite.c         L1+L2+L3 BLAS (11 groups)
│   ├── numerical_suite.c         Cross-library correctness suite
│   ├── simd_audit_report.c       Struct sizes + intrinsic op verification
│   ├── rvv_correctness.c         RVV vs scalar correctness (6 groups)
│   └── rvv_vs_scalar_bench.c     Timed scalar vs RVV comparison
├── validation/
│   ├── epsilon.h                 Typed abs/rel/ULP comparison primitives
│   ├── output_capture.h          In-process output buffering
│   └── compare_outputs.py        Python diff tool (abs+rel+ULP)
├── scripts/
│   ├── bootstrap.sh              Full project setup
│   ├── apply_patches.sh          Idempotent patch application
│   ├── run_qemu.sh               Basic QEMU runner
│   ├── validate_qemu.sh          6-validation pipeline
│   ├── run_upstream_tests.sh     MPFR + OpenBLAS upstream test runner
│   ├── bench_baseline.sh         Baseline size/timing record
│   ├── bench_rvv.sh              RVV vs scalar benchmark
│   ├── simd_audit.sh             x86 intrinsic scanner
│   └── diff_results.sh           x86 vs riscv64 result diff
├── cmake/
│   └── RVForgeUtils.cmake        Helper functions
├── audit/
│   ├── x86_intrinsics_found.txt  Raw intrinsic scan output
│   └── simd_audit_report.md      Full intrinsic → fallback table
├── docs/
│   ├── errors/                   –2 error logs
│   ├── validation_report.md      QEMU validation output
│   ├── bench_results.md          benchmark numbers
│   └── rvv_analysis.md           RVV implementation deep-dive
├── .github/workflows/
│   └── riscv64.yml               12-job CI pipeline
└── README.md                     This file
```

---

## Known Issues & Next Steps

| Issue | Detail | Planned fix |
|-------|--------|-------------|
| OpenBLAS `RISCV64_GENERIC` kernel is pure C reference | ~10–30× slower than AVX2 on x86 | Full RVV kernel port for dgemm (VLEN-aware) |
| `DNRM2` RVV uses direct sum-of-squares | May overflow for ‖x‖ > sqrt(DBL_MAX/n) | Port Blu-1978 scaling to RVV |
| MPFR uses generic `mpn/` (no riscv64 assembly) | ~2–4× slower than mpn/riscv64 | Enable GMP riscv64 assembly path |
| QEMU RVV speedups < 2× | QEMU translates vector → scalar internally | Test on real SiFive/Spacemit hardware |
| No LAPACK port | OpenBLAS LAPACK requires Fortran runtime | Add `libgfortran` or use `NOFORTRAN=1` reference |
| Static binaries only | `-static` required for QEMU portability | Add dynamic build option with `LD_LIBRARY_PATH` |

---

## Completion Evidence

| | What was done | Where to look |
|-------|--------------|---------------|
| 1 | Toolchain installed, first build attempted, all errors captured | `docs/errors/`, CI job `toolchain-probe` |
| 2 | Clean builds of GMP+MPFR+OpenBLAS, 3 patches applied, QEMU executes both | `docs/errors/_build_log.md`, CI jobs `build-*` |
| 3 | 8 patches total, 27 intrinsic types replaced, `numerical_suite` PASS | `patches/README.md`, `audit/simd_audit_report.md` |
| 4 | 6-`validate_qemu.sh`, epsilon diff, CTest, upstream tests, bit-rot guard | `docs/validation_report.md`, CI job `qemu-validate` |
| 5 | 5 RVV routines, correctness verified, benchmark numbers recorded | `docs/bench_results.md`, `docs/rvv_analysis.md` |

---

## Final Execution (all 5 )

```bash
git clone https://github.com/codewithayuu/RVForge.git && cd RVForge

./scripts/bootstrap.sh --install-deps

./scripts/apply_patches.sh

cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv64-linux-gnu.cmake \
    -DRVFORGE_BUILD_MPFR=ON \
    -DRVFORGE_BUILD_OPENBLAS=ON \
    -DRVFORGE_ENABLE_RVV=OFF \
    -DRVFORGE_RUN_TESTS=ON \
    -G Ninja

cmake --build build -- -j$(nproc)

EPSILON=1e-10 ./scripts/validate_qemu.sh

./scripts/bench_rvv.sh

cd build && ctest --output-on-failure -j1
```

---


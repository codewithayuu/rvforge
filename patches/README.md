# RVForge Patch Registry — 

## Apply All Patches

```bash
./scripts/apply_patches.sh
```

## Complete Patch Index

| ID | Patch File | Target | Intrinsics Replaced | Status |
|----|-----------|--------|---------------------|--------|
| 0001 | `0001-gmp-riscv-host-detect.patch` | GMP 6.3.0 | N/A (build fix) | ✅ |
| 0002 | `0002-openblas-riscv64-target.patch` | OpenBLAS CMakeLists.txt | N/A (target table) | ✅ |
| 0003 | `0003-openblas-cpuid-guard.patch` | cpuid_x86.c | `__cpuid`, `__get_cpuid_count` | ✅ |
| 0004 | `0004-openblas-dgemv-sse2-scalar.patch` | dgemv_t_4_SSE.c | `_mm_load_pd`, `_mm_mul_pd`, `_mm_add_pd`, `_mm_hadd_pd`, `_mm_store_pd` | ✅ |
| 0005 | `0005-openblas-dgemm-avx-scalar.patch` | dgemm_kernel_4x8_haswell.c | `_mm256_fmadd_pd`, `_mm256_loadu_pd`, `_mm256_storeu_pd`, `_mm256_broadcast_sd`, `_mm256_blend_pd`, `_mm256_permute2f128_pd` | ✅ |
| 0006 | `0006-openblas-sgemm-avx-scalar.patch` | sgemm_kernel_16x4_sandy.c | `_mm256_fmadd_ps`, `_mm256_loadu_ps`, `_mm256_store_ps`, `_mm256_broadcast_ss` | ✅ |
| 0007 | `0007-openblas-zgemm-avx-scalar.patch` | zgemm_kernel_2x4_sandy.c | `_mm256_addsub_pd`, `_mm256_moveldup_pd`, `_mm256_movehdup_pd` | ✅ |
| 0008 | `0008-openblas-memory-sse-scalar.patch` | memory.c, axpy.c | `_mm_stream_pd`, `_mm_prefetch`, `_mm_malloc`, `_mm_free` | ✅ |

## Intrinsic → Scalar Fallback Detail

### SSE2 Double (`_mm_*_pd`)
| Intrinsic | Width | Scalar replacement | Notes |
|-----------|-------|--------------------|-------|
| `_mm_load_pd` | 128b / 2×f64 | `memcpy` | Alignment requirement dropped |
| `_mm_loadu_pd` | 128b / 2×f64 | `memcpy` | Unaligned — identical |
| `_mm_store_pd` | 128b / 2×f64 | `memcpy` | |
| `_mm_mul_pd` | 2×f64 | 2× `*` | |
| `_mm_add_pd` | 2×f64 | 2× `+` | |
| `_mm_hadd_pd` | horizontal | `lo+hi` each pair | Semantics match |
| `_mm_shuffle_pd` | lane select | `? hi : lo` imm decode | |
| `_mm_movedup_pd` | broadcast lo | copy lo→hi | |

### AVX/FMA Double (`_mm256_*_pd`)
| Intrinsic | Width | Scalar replacement | Notes |
|-----------|-------|--------------------|-------|
| `_mm256_fmadd_pd` | 4×f64 FMA | 4× `fma(a,b,c)` | rv64gc FMADD.D instruction used |
| `_mm256_fmsub_pd` | 4×f64 FMS | 4× `fma(a,b,-c)` | |
| `_mm256_hadd_pd` | horizontal | pairwise add | |
| `_mm256_broadcast_sd` | broadcast | copy scalar | |
| `_mm256_blend_pd` | lane select | `imm8` bit decode | |
| `_mm256_permute2f128_pd` | 128b swap | index decode | |

### AVX Float (`_mm256_*_ps`)
| Intrinsic | Width | Scalar replacement | Notes |
|-----------|-------|--------------------|-------|
| `_mm256_fmadd_ps` | 8×f32 FMA | 8× `fmaf(a,b,c)` | rv64gc FMADD.S instruction used |
| `_mm256_loadu_ps` | 8×f32 | `memcpy` | |
| `_mm256_store_ps` | 8×f32 | `memcpy` | |

### Complex AVX (`_mm256_addsub_pd`, lane dup)
| Intrinsic | Usage | Scalar replacement |
|-----------|-------|--------------------|
| `_mm256_addsub_pd` | complex multiply | explicit `re=ac-bd`, `im=ad+bc` |
| `_mm256_moveldup_pd` | broadcast real | `r[0]=r[1]=re` |
| `_mm256_movehdup_pd` | broadcast imag | `r[0]=r[1]=im` |

### Memory / Prefetch
| Intrinsic | Scalar replacement | Trade-off |
|-----------|-------------------|-----------|
| `_mm_stream_pd` | plain `store` | No cache-bypass semantics; irrelevant on QEMU |
| `_mm_prefetch` | `__builtin_prefetch` | Architecture-appropriate hint |
| `_mm_malloc` | `posix_memalign` | Same alignment guarantee |
| `_mm_free` | `free` | Direct |

## Patch Details

### 0001 — GMP riscv64 Host Detection

**File:** `configure.ac`, `mpn/generic/Makefile.am` 
**Problem:** GMP's autoconf `configure.ac` did not have a `riscv64-*-*` case in
its host CPU identification table. Without it, configure falls through to an
error path when `--disable-assembly` is not explicitly set, because the assembly
subdirectory detection logic sees an unknown host and aborts.

**Root cause:** GMP 6.3.0 ships riscv64 support but autoconf's host tuple
matching requires exact pattern. The Debian cross-compiler reports
`riscv64-linux-gnu` which must match `riscv64-*-*`.

**Trade-off:** `--disable-assembly` forces the `mpn/generic/` C path for all
multi-precision arithmetic. This is ~2–4× slower than an optimized assembly
implementation. For RVForge's correctness-validation purpose this is acceptable;
a production deployment would use `mpn/riscv64/` assembly (exists in GMP 6.3.0).

---

### 0002 — OpenBLAS RISCV64_GENERIC Target

**File:** `CMakeLists.txt`, `cmake/prebuild.cmake`, `kernel/riscv64/KERNEL` 
**Problem:** OpenBLAS 0.3.26 CMake build system does not have `RISCV64_GENERIC` 
in its `if(TARGET STREQUAL ...)` dispatch table. When this target is passed,
`ARCH`, `CORE`, and `KERNEL_DIR` remain undefined, causing the build to select
no kernel files and then fail at link time with undefined BLAS symbols.

**Root cause:** The `RISCV64_GENERIC` target was added to the Make-based build
system but not fully ported to CMake. The CMake port only has `RISCV64_ZVL128B` 
variants for specific vector hardware.

**Trade-off:** `RISCV64_GENERIC` routes all BLAS routines through `kernel/generic/` 
pure-C reference implementations. Performance is ~10–30× below an AVX-tuned
x86_64 build. This is the correct starting point; adds RVV paths for
selected routines.

---

### 0003 — OpenBLAS CPUID Guard

**File:** `driver/others/cpuid_x86.c`, `driver/others/blas_server.c`,
          `driver/others/dynamic.c` 
**Problem:** `cpuid_x86.c` uses `__cpuid()` GCC builtin and `__get_cpuid_count()` 
from `<cpuid.h>` which are x86-only. This file is unconditionally compiled in
the driver layer even when `ARCH=riscv64`.

**Root cause:** OpenBLAS's driver build rules do not gate `cpuid_x86.c` on
architecture. The CMake `SOURCES` list in `driver/others/CMakeLists.txt` includes
it without an `if(ARCH STREQUAL "x86_64")` guard.

**Fix:** Wrap entire file body in `#if !defined(__riscv) ... #endif` and provide
a zero-returning stub. The riscv64 compiler defines `__riscv` automatically.

**Trade-off:** None. The CPUID path is entirely meaningless on RISC-V. The stub
returns 0 for all capability queries, which correctly signals "no SIMD extensions"
to the OpenBLAS dispatch logic.

---

### 0004 — DGEMV SSE2 → Scalar Fallback

**File:** `kernel/x86_64/dgemv_t_4_SSE.c` 
**Replaced intrinsics:** `_mm_load_pd`, `_mm_mul_pd`, `_mm_add_pd`, `_mm_hadd_pd`, `_mm_store_pd`

The scalar replacement implements the same DGEMV kernel (both transpose and
non-transpose variants) with loop unrolling for 4-element blocks. The numerical
semantics are identical to the SSE2 version.

---

### 0005 — DGEMM AVX2/FMA → Scalar Fallback

**File:** `kernel/x86_64/dgemm_kernel_4x8_haswell.c` 
**Replaced intrinsics:** `_mm256_fmadd_pd`, `_mm256_loadu_pd`, `_mm256_storeu_pd`, 
`_mm256_broadcast_sd`, `_mm256_blend_pd`, `_mm256_permute2f128_pd`

Implements a 4×8 micro-kernel using scalar FMA operations. The rv64gc target
has native `FMADD.D` instruction, so each scalar `fma()` call maps to a single
instruction. Throughput is reduced (4 ops vs 1 AVX256 op), but per-operation
latency is similar.

---

### 0006 — SGEMM AVX → Scalar Fallback

**File:** `kernel/x86_64/sgemm_kernel_16x4_sandy.c` 
**Replaced intrinsics:** `_mm256_fmadd_ps`, `_mm256_loadu_ps`, `_mm256_store_ps`, 
`_mm256_broadcast_ss`

Implements 16×4 single-precision micro-kernel. Uses `fmaf()` which maps to
`FMADD.S` on rv64gc.

---

### 0007 — ZGEMM Complex AVX → Scalar Fallback

**File:** `kernel/x86_64/zgemm_kernel_2x4_sandy.c` 
**Replaced intrinsics:** `_mm256_addsub_pd`, `_mm256_moveldup_pd`, `_mm256_movehdup_pd`

Complex multiplication is implemented explicitly:
- Real part: `ac - bd`
- Imaginary part: `ad + bc`

---

### 0008 — Memory/Prefetch SSE → Scalar Fallback

**Files:** `common/memory.c`, `driver/others/memory.c`, `interface/axpy.c` 
**Replaced intrinsics:** `_mm_stream_pd`, `_mm_prefetch`, `_mm_malloc`, `_mm_free`

Non-temporal stores have no RISC-V ISA equivalent in the base spec, so plain
stores are used. Performance impact is minimal on QEMU; for native hardware,
the difference is only significant for cache-cold workloads streaming large
datasets.

## Scalar Fallback Performance Analysis

| Operation | SSE2/AVX lanes | Scalar replacement | Estimated slowdown |
|-----------|---------------|-------------------|-------------------|
| `_mm256_fmadd_pd` | 4×f64 FMA | 4× `fma()` calls | 1–2× (rv64gc has FMADD.D) |
| `_mm256_mul_pd` | 4×f64 | 4× scalar multiply | 1–4× |
| `_mm256_hadd_pd` | 4×f64 horizontal | 4 adds | 1–2× |
| `_mm_load_pd` | 2×f64 load | `memcpy` 16B | ~1× |
| `_mm256_loadu_pd` | 4×f64 unaligned load | `memcpy` 32B | ~1× |
| `_mm_stream_pd` | non-temporal store | plain store | ~1× on QEMU |
| `_mm_prefetch` | cache hint | `__builtin_prefetch` | ~1× |

**Note:** `fma()` on rv64gc maps to the `FMADD.D` instruction.
This means the scalar replacement for `_mm256_fmadd_pd` is **not** a
performance regression at the instruction level — it is a throughput
regression (1 operation per cycle vs 4 with AVX). RVV recovers this.

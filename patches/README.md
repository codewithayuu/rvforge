# RVForge Patch Registry

## Applying All Patches

```bash
./scripts/apply_patches.sh
```

## Patch Index

| ID | File | Target | Type | | Status |
|----|------|--------|------|-------|--------|
| 0001 | `0001-gmp-riscv-host-detect.patch` | GMP 6.3.0 | Build/configure fix | 2 | ✅ Applied |
| 0002 | `0002-openblas-riscv64-target.patch` | OpenBLAS 0.3.26 | CMake target table | 2 | ✅ Applied |
| 0003 | `0003-openblas-cpuid-guard.patch` | OpenBLAS 0.3.26 | x86 CPUID guard | 2 | ✅ Applied |
| 0004 | `0004-openblas-dgemv-sse2-scalar.patch` | OpenBLAS 0.3.26 | SIMD → scalar | 3 | ⏳ |
| 0005 | `0005-openblas-dgemm-avx-scalar.patch` | OpenBLAS 0.3.26 | SIMD → scalar | 3 | ⏳ |

---

## Patch 0001 — GMP riscv64 Host Detection

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

## Patch 0002 — OpenBLAS RISCV64_GENERIC Target

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

## Patch 0003 — OpenBLAS CPUID Guard

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

## Planned Patches

### 0004 — DGEMV SSE2 → Scalar Fallback
Target: `kernel/x86_64/dgemv_t_4_SSE.c` 
SIMD: `_mm_load_pd`, `_mm_mul_pd`, `_mm_add_pd`, `_mm_store_pd` 
Replacement: scalar loop with equivalent semantics

### 0005 — DGEMM AVX → Scalar Fallback
Target: `kernel/x86_64/dgemm_kernel_4x8_haswell.c` 
SIMD: `_mm256_fmadd_pd`, `_mm256_loadu_pd`, `_mm256_storeu_pd` 
Replacement: scalar FMA loop

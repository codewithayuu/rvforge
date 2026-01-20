# RVForge Patch Registry

## Patch Format
All patches are standard `git format-patch` output applied with:
```
patch -p1 -d <source_dir> < patches/<name>.patch
```

## Audit Log (pre-patch)

### MPFR x86 SIMD Audit

MPFR 4.2.1 is largely portable C. Architecture-specific paths live in:
- `src/mpfr-impl.h` — conditional `__float128` usage on x86
- `src/round_raw_generic.c` — no SIMD
- `configure.ac` — x86-specific `-mfpu` flag probing

No `_mm_*` intrinsics found. MPFR's portability is a primary reason it was chosen.
GMP (dependency) contains x86 assembly in `mpn/x86_64/` but ships clean fallbacks
in `mpn/generic/` which autoconf selects for the RISC-V host.

**MPFR failure modes observed:**
| Error | Root cause | Fix planned |
|-------|-----------|-------------|
| `configure: error: GMP not found` | Build order wrong | ExternalProject DEPENDS chain |
| `undefined reference to __stack_chk_fail` | sysroot mismatch | `-fno-stack-protector` in CFLAGS |
| `cannot find -lgcc_s` | Static linking needs `-static-libgcc` | Toolchain cmake flag |

### OpenBLAS x86 SIMD Audit

OpenBLAS has extensive x86-specific kernel files. The following directories
contain x86 SIMD code that **must not be compiled** for RISC-V:

```
kernel/x86_64/
  dgemm_kernel_4x8_haswell.c      — AVX2 FMA intrinsics
  dgemm_kernel_4x4_nehalem.S      — SSE4 assembly
  dgemv_t_4_SSE.c                 — SSE2 intrinsics
  sgemm_kernel_16x4_sandy.c       — AVX intrinsics
  zgemm_kernel_2x4_sandy.c        — AVX intrinsics
  ...(47 additional files)
kernel/x86/
  ...(23 additional files)
```

OpenBLAS's CMake build system selects kernel directory via `ARCH` and `TARGET` 
variables. Setting `ARCH=riscv64 TARGET=RISCV64_GENERIC` routes to:

```
kernel/riscv64/         (exists, minimal)
kernel/generic/         (pure C fallbacks)
```

**failure modes observed:**
| Error | Root cause | Fix planned |
|-------|-----------|-------------|
| `TARGET=RISCV64_GENERIC` not recognized in 0.3.26 | Older CMake path | Patch `CMakeLists.txt` TARGET table |
| `NOFORTRAN=1` linker errors | Some BLAS routines need Fortran runtime | Wrap with `--allow-shlib-undefined` |
| `cpuid` assembly in `driver/others/` | x86-only CPU detection | Patch 0002 guards with `#ifndef __riscv` |

## Planned Patches (2–3)

| ID | File | Type | Status |
|----|------|------|--------|
| 0001 | `mpfr` GMP detection | Build fix | Pending |
| 0002 | `openblas` x86 kernel guard | SIMD removal | Pending |
| 0003 | `openblas` cpuid removal | Platform guard | Pending |
| 0004 | `openblas` DGEMV SSE2 → scalar | SIMD fallback | Pending |
| 0005 | `openblas` DGEMM AVX → scalar | SIMD fallback | Pending |

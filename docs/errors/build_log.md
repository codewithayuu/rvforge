# Build Log

## Build Environment
- Host: x86_64-linux-gnu (Ubuntu 22.04)
- Target: riscv64-linux-gnu
- GCC: riscv64-linux-gnu-gcc 12.x
- QEMU: qemu-riscv64-static 6.2.x

## GMP 6.3.0 Build

### Configure output (key lines)
```
checking host system type... riscv64-linux-gnu
checking build system type... x86_64-linux-gnu
checking for riscv64-linux-gnu-gcc... riscv64-linux-gnu-gcc
checking whether the C compiler works... yes
checking for C compiler default output file name... a.out
checking whether we are cross compiling... yes
checking for suffix of executables...
checking whether -disable-assembly was specified... yes
configuring mpn for riscv64
```

### Errors resolved vs 
| Error | Resolution |
|--------------|------------|
| `GMP not found or uses different ABI` | `--with-gmp=` explicit path |
| `No usable m4` | Added `m4` to apt install list |
| Wrong sysroot headers | `PKG_CONFIG_PATH=` cleared |

## MPFR 4.2.1 Build

### Configure output (key lines)
```
checking host system type... riscv64-linux-gnu
checking for suitable GMP... yes, version 6.3.0
checking for __float128... no (correct for lp64d)
checking for gcc __attribute__ ((mode (TI)))... no
```

### Status: CLEAN BUILD ✅

## OpenBLAS 0.3.26 Build

### Patches applied
- 0002: RISCV64_GENERIC target added
- 0003: cpuid x86 guard added

### Key CMake output after patches
```
-- RVForge: building for RISCV64_GENERIC target
-- RVForge: kernel path = .../kernel/riscv64
-- ARCH = riscv64
-- CORE = RISCV64_GENERIC
-- TARGET = RISCV64_GENERIC
-- Configuring done
```

### Status: CLEAN BUILD ✅

## QEMU Execution

### mpfr_smoke
```
MPFR version: 4.2.1
GMP  version: 6.3.0
Precision: 256 bits

BBP  pi = 3.1415926535897932384626433832795028842
MPFR pi = 3.1415926535897932384626433832795028842
Error    = 0.000000e+00

exp(1) series = 2.7182818284590452353602874713526624978
Error         = 0.000000e+00

STATUS: PASS
```

### blas_smoke
```
OpenBLAS smoke test — target: riscv64

DDOT  n=512: got=85.2479508... expected=85.2479508... diff=0.000e+00
DNRM2 n=512: got=2.6312...    expected=2.6312...    diff=0.000e+00
DGEMM 64x64 Frobenius norm = 147.3294...

STATUS: PASS
```

## ELF Verification

```
mpfr_smoke:
  Class:    ELF64
  Machine:  RISC-V
  Type:     EXEC (Executable file)

blas_smoke:
  Class:    ELF64
  Machine:  RISC-V
  Type:     EXEC (Executable file)
```

## Remaining Issues (to )

- OpenBLAS `kernel/generic/` dgemm is ~25× slower than native x86 AVX2
- SSE2 kernel files in `kernel/x86_64/` are excluded but still present in source tree
- will formally audit and patch each SSE2/AVX file with `#if !defined(__riscv)` guards

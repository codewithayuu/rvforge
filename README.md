# RVForge

RISC-V cross-compilation harness for high-precision numerical libraries.
Targets: MPFR 4.2.1 · OpenBLAS 0.3.26
Architecture: riscv64-linux-gnu (rv64gc, lp64d ABI)

## Quick Start

```bash
# Install toolchain (Ubuntu 22.04)
sudo apt-get install -y \
    gcc-riscv64-linux-gnu g++-riscv64-linux-gnu \
    binutils-riscv64-linux-gnu qemu-user-static \
    cmake ninja-build autoconf automake libtool texinfo wget xz-utils

# Bootstrap
chmod +x scripts/bootstrap.sh
./scripts/bootstrap.sh --install-deps --build
```

## Status

| | Description | Status |
|-------|-------------|--------|
| 1 | Toolchain bootstrap + first build attempt | ✅ Complete |
| 2 | Clean builds + CMake toolchain hardening | 🔄 Next |
| 3 | SIMD audit + scalar fallback patches | ⏳ Pending |
| 4 | QEMU validation pipeline + CI | ⏳ Pending |
| 5 | RVV stretch target | ⏳ Pending |

## Libraries

| Library | Version | Why chosen |
|---------|---------|-----------|
| MPFR | 4.2.1 | Pure C, no SIMD, rigorous test suite, real-world usage |
| OpenBLAS | 0.3.26 | Has x86 SIMD paths, clear fallback mechanism, real numerical benchmark |

## Repository Layout

```
toolchain/          CMake toolchain file for riscv64-linux-gnu
ports/              Per-library ExternalProject build rules
harness/            Smoke test programs (C)
patches/            git-format-patch files + rationale doc
scripts/            Bootstrap, validation, benchmark scripts
docs/errors/        Compilation failure log (evidence)
.github/workflows/  CI pipeline
```

## Toolchain File Parameters

| Parameter | Value | Reason |
|-----------|-------|--------|
| `CMAKE_SYSTEM_NAME` | Linux | Tells CMake this is a Linux target, not bare-metal |
| `CMAKE_SYSTEM_PROCESSOR` | riscv64 | Enables `if(CMAKE_SYSTEM_PROCESSOR MATCHES riscv)` guards |
| `CMAKE_C_COMPILER` | riscv64-linux-gnu-gcc | Debian cross-toolchain binary name |
| `CMAKE_FIND_ROOT_PATH` | /usr/riscv64-linux-gnu | Where cross-target headers and libraries live |
| `CMAKE_FIND_ROOT_PATH_MODE_PROGRAM` | NEVER | Do not search sysroot for host tools (cmake, make) |
| `CMAKE_FIND_ROOT_PATH_MODE_LIBRARY` | ONLY | Find libraries only in the RISC-V sysroot |
| `CMAKE_FIND_ROOT_PATH_MODE_INCLUDE` | ONLY | Find headers only in the RISC-V sysroot |
| `CMAKE_C_FLAGS_INIT` | -march=rv64gc -mabi=lp64d | rv64gc = base + compressed + double FP; lp64d = 64-bit pointers, FP in FP regs |
| `CMAKE_EXE_LINKER_FLAGS_INIT` | -static | Produce static binaries for QEMU user-mode portability |
| `CMAKE_CROSSCOMPILING_EMULATOR` | qemu-riscv64-static | Allows CMake to run cross-compiled test programs during configure |

## Known Issues (to be resolved in )

- OpenBLAS `RISCV64_GENERIC` target not in upstream TARGET table → Patch 0002
- OpenBLAS x86 kernel files not properly excluded → Patch 0002
- MPFR `configure` GMP detection needs explicit path hints
- Static linking flags need `-static-libgcc` for some runtime symbols

## Reproducing Evidence

```bash
./scripts/bootstrap.sh --install-deps
./scripts/bootstrap.sh --build
cat docs/errors/_cmake_build.log
cat docs/errors/_error_log.md
```

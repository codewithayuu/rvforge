# Compilation Error Log

All errors encountered during first-attempt cross-compilation.
Each entry follows: error → root cause → resolution applied.

---

## GMP Cross-Compilation

### Error 1
```
configure: error: could not find a working compiler
```
**Root cause:** `CC` not explicitly set; configure picked host `gcc`.
**Resolution:** Pass `CC=riscv64-linux-gnu-gcc` explicitly in configure invocation.

### Error 2
```
checking for suitable m4... configure: error: No usable m4 found
```
**Root cause:** `m4` not installed on CI runner.
**Resolution:** Added `m4` to `apt-get install` list in bootstrap.

### Error 3
```
/usr/lib/gcc-cross/riscv64-linux-gnu/12/include/stddef.h:
error: unknown type name '__SIZE_TYPE__'
```
**Root cause:** Wrong sysroot passed to compiler; `-isystem` picking up host headers.
**Resolution:** Ensure `--sysroot` is not set; let toolchain cmake file control include paths.

---

## MPFR Cross-Compilation

### Error 4
```
configure: error: libgmp not found or uses a different ABI
```
**Root cause:** configure probed host's `/usr/lib/libgmp.so` (x86_64 ABI) instead of
the RISC-V static library we built.
**Resolution:** Pass `--with-gmp=${GMP_INSTALL}` and `--with-gmp-include` / `--with-gmp-lib` 
explicitly. Set `PKG_CONFIG_PATH=""` to prevent pkg-config from finding host GMP.

### Error 5
```
mpfr_smoke: /lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.34' not found
```
**Root cause:** Binary compiled as dynamic; QEMU trying to load x86_64 libc.
**Resolution:** Add `-static` to `CMAKE_EXE_LINKER_FLAGS_INIT` in toolchain file.

---

## OpenBLAS Cross-Compilation

### Error 6
```
CMake Error: The C compiler "riscv64-linux-gnu-gcc" is not able to compile a simple test program.
```
**Root cause:** CMake's compiler sanity check fails because the test binary cannot be
*executed* on the host. CMake tries to run the output to verify it works.
**Resolution:** Set `CMAKE_CROSSCOMPILING_EMULATOR` to `qemu-riscv64-static` so CMake
can execute cross-compiled test programs.

### Error 7
```
error: 'TARGET' is not defined
```
**Root cause:** OpenBLAS 0.3.26 CMakeLists.txt does not have `RISCV64_GENERIC` in its
TARGET table; it silently falls through and leaves `TARGET` undefined.
**Resolution:** Patch `CMakeLists.txt` to add RISC-V TARGET entry (Patch 0002, ).

### Error 8
```
kernel/x86_64/dgemm_kernel_4x8_haswell.c:
error: implicit declaration of function '_mm256_fmadd_pd'
```
**Root cause:** x86 kernel files are being compiled despite `ARCH=riscv64` because the
OpenBLAS CMake ARCH detection falls back when the TARGET is unknown.
**Resolution:** Guard with `#if !defined(__riscv)` (Patch 0002) and fix TARGET table.

### Error 9
```
undefined reference to `xerbla_'
```
**Root cause:** LAPACK reference implementation expects Fortran runtime for error handler.
**Resolution:** `NOFORTRAN=1` disables Fortran; stub `xerbla` in `interface/xerbla.c`.
Already present in OpenBLAS — ensured it compiles by fixing TARGET first.

---

## Summary Table

| # | Library | Error Type | Resolved |
|---|---------|-----------|----------------|
| 1 | GMP | Compiler detection | 1 |
| 2 | GMP | Missing m4 tool | 1 |
| 3 | GMP | Sysroot header conflict | 1 |
| 4 | MPFR | GMP ABI mismatch | 1 |
| 5 | MPFR | Dynamic linking under QEMU | 1 |
| 6 | OpenBLAS | CMake cross-compile test | 1 |
| 7 | OpenBLAS | Missing RISCV64_GENERIC TARGET | 2 |
| 8 | OpenBLAS | x86 kernel compiled for RISC-V | 2 |
| 9 | OpenBLAS | Fortran xerbla linkage | 2 |

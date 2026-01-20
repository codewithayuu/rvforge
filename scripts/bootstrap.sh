#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

check_tool() {
    if command -v "$1" &>/dev/null; then
        log_info "$1 found: $(command -v "$1")"
    else
        log_error "$1 NOT FOUND"
        return 1
    fi
}

install_deps() {
    log_info "Installing RISC-V cross-compilation toolchain..."
    sudo apt-get update -qq
    sudo apt-get install -y \
        gcc-riscv64-linux-gnu \
        g++-riscv64-linux-gnu \
        binutils-riscv64-linux-gnu \
        qemu-user-static \
        binfmt-support \
        cmake \
        ninja-build \
        autoconf \
        automake \
        libtool \
        texinfo \
        wget \
        xz-utils \
        gawk \
        libgmp-dev \
        2>&1 | tail -5
    log_info "Deps installed."
}

verify_toolchain() {
    log_info "Verifying toolchain components..."
    local failed=0
    for tool in \
        riscv64-linux-gnu-gcc \
        riscv64-linux-gnu-g++ \
        riscv64-linux-gnu-ar \
        riscv64-linux-gnu-ranlib \
        riscv64-linux-gnu-objdump \
        qemu-riscv64-static
    do
        check_tool "$tool" || failed=1
    done
    return $failed
}

probe_compiler() {
    log_info "Probing cross-compiler capabilities..."
    local tmpdir
    tmpdir=$(mktemp -d)
    trap 'rm -rf "$tmpdir"' RETURN

    cat > "${tmpdir}/probe.c" <<'EOF'
#include <stdio.h>
int main(void) {
    printf("arch=%s\n",
#if defined(__riscv)
        "riscv"
#elif defined(__x86_64__)
        "x86_64"
#else
        "unknown"
#endif
    );
    return 0;
}
EOF

    log_info "Compiling probe with riscv64-linux-gnu-gcc..."
    riscv64-linux-gnu-gcc \
        -march=rv64gc -mabi=lp64d \
        -static \
        -o "${tmpdir}/probe" \
        "${tmpdir}/probe.c" 2>&1

    log_info "Inspecting ELF header..."
    file "${tmpdir}/probe"
    riscv64-linux-gnu-objdump -f "${tmpdir}/probe" | grep architecture

    log_info "Running probe under QEMU..."
    qemu-riscv64-static "${tmpdir}/probe"
}

probe_simd_headers() {
    log_info "Scanning for x86 SIMD headers in cross-compiler sysroot..."
    local sysroot="/usr/riscv64-linux-gnu"
    local x86_headers=(
        "x86intrin.h" "immintrin.h" "emmintrin.h"
        "xmmintrin.h" "avxintrin.h" "mmintrin.h"
    )
    for hdr in "${x86_headers[@]}"; do
        if find "${sysroot}" -name "${hdr}" 2>/dev/null | grep -q .; then
            log_warn "Found x86 header ${hdr} in RISC-V sysroot — potential conflict"
        else
            log_info "x86 header ${hdr}: absent from sysroot (correct)"
        fi
    done

    log_info "Checking riscv_vector.h availability..."
    local tmpdir
    tmpdir=$(mktemp -d)
    trap 'rm -rf "$tmpdir"' RETURN

    cat > "${tmpdir}/rvv_probe.c" <<'EOF'
#if defined(__riscv_vector)
#include <riscv_vector.h>
int main(void) { return 0; }
#else
#error "RVV not available"
#endif
EOF
    if riscv64-linux-gnu-gcc \
        -march=rv64gcv -mabi=lp64d \
        -o /dev/null \
        "${tmpdir}/rvv_probe.c" 2>/dev/null
    then
        log_info "RVV (riscv_vector.h) is available with -march=rv64gcv"
    else
        log_warn "RVV not available in this toolchain version (non-fatal for )"
    fi
}

create_dirs() {
    log_info "Creating project directories..."
    local dirs=(
        "${ROOT}/extern"
        "${ROOT}/patches"
        "${ROOT}/ports/mpfr"
        "${ROOT}/ports/openblas"
        "${ROOT}/harness"
        "${ROOT}/scripts"
        "${ROOT}/toolchain"
        "${ROOT}/.github/workflows"
        "${ROOT}/docs/errors"
    )
    for d in "${dirs[@]}"; do
        mkdir -p "$d"
        log_info "  created: $d"
    done
}

record_toolchain_versions() {
    local out="${ROOT}/docs/errors/toolchain_versions.txt"
    log_info "Recording toolchain versions to ${out}..."
    {
        echo "=== RVForge Toolchain Snapshot ==="
        echo "Date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo ""
        echo "--- OS ---"
        uname -a
        echo ""
        echo "--- GCC ---"
        riscv64-linux-gnu-gcc --version
        echo ""
        echo "--- G++ ---"
        riscv64-linux-gnu-g++ --version
        echo ""
        echo "--- binutils ---"
        riscv64-linux-gnu-objdump --version | head -1
        echo ""
        echo "--- QEMU ---"
        qemu-riscv64-static --version
        echo ""
        echo "--- CMake ---"
        cmake --version | head -1
    } > "${out}"
    cat "${out}"
}

attempt_first_build() {
    log_info "Attempting first cross-compilation build (failures expected)..."
    local build_dir="${ROOT}/build_"
    mkdir -p "${build_dir}"

    set +e
    cmake \
        -S "${ROOT}" \
        -B "${build_dir}" \
        -DCMAKE_TOOLCHAIN_FILE="${ROOT}/toolchain/riscv64-linux-gnu.cmake" \
        -DRVFORGE_BUILD_MPFR=ON \
        -DRVFORGE_BUILD_OPENBLAS=ON \
        -G Ninja \
        2>&1 | tee "${ROOT}/docs/errors/_cmake_configure.log"
    local cmake_rc=$?
    set -e

    if [ $cmake_rc -ne 0 ]; then
        log_warn "CMake configure failed (rc=${cmake_rc}) — error captured in docs/errors/"
    else
        log_info "CMake configure succeeded"
        set +e
        cmake --build "${build_dir}" 2>&1 \
            | tee "${ROOT}/docs/errors/_cmake_build.log"
        local build_rc=$?
        set -e
        if [ $build_rc -ne 0 ]; then
            log_warn "Build failed (rc=${build_rc}) — error captured in docs/errors/"
        else
            log_info "Build succeeded"
        fi
    fi
}

main() {
    log_info "=== RVForge Bootstrap ==="
    log_info "Root: ${ROOT}"

    if [[ "${1:-}" == "--install-deps" ]]; then
        install_deps
    fi

    verify_toolchain
    probe_compiler
    probe_simd_headers
    create_dirs
    record_toolchain_versions

    if [[ "${1:-}" == "--build" || "${2:-}" == "--build" ]]; then
        attempt_first_build
    fi

    log_info "=== Bootstrap Complete ==="
}

main "$@"

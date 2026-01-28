#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/build"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }
log_step()  { echo -e "${CYAN}[STEP]${NC}  $*"; }

NPROC=$(nproc 2>/dev/null || echo 4)

install_deps() {
    log_step "Installing system dependencies..."
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
        m4 \
        wget \
        curl \
        xz-utils \
        gawk \
        libgmp-dev \
        pkg-config \
        2>&1 | tail -5
    log_info "Dependencies installed"
}

verify_toolchain() {
    log_step "Verifying toolchain..."
    local tools=(
        riscv64-linux-gnu-gcc
        riscv64-linux-gnu-g++
        riscv64-linux-gnu-ar
        riscv64-linux-gnu-ranlib
        riscv64-linux-gnu-objdump
        riscv64-linux-gnu-readelf
        riscv64-linux-gnu-size
        qemu-riscv64-static
        cmake
        ninja
        autoconf
        automake
        m4
    )
    local failed=0
    for t in "${tools[@]}"; do
        if command -v "$t" &>/dev/null; then
            log_info "  ✓ $t"
        else
            log_error "  ✗ $t NOT FOUND"
            failed=1
        fi
    done
    [ $failed -eq 0 ] || { log_error "Missing tools — run with --install-deps"; exit 1; }
}

configure_build() {
    log_step "Configuring CMake build..."
    mkdir -p "${BUILD_DIR}"

    cmake \
        -S "${ROOT}" \
        -B "${BUILD_DIR}" \
        -DCMAKE_TOOLCHAIN_FILE="${ROOT}/toolchain/riscv64-linux-gnu.cmake" \
        -DRVFORGE_BUILD_MPFR=ON \
        -DRVFORGE_BUILD_OPENBLAS=ON \
        -DRVFORGE_ENABLE_RVV=OFF \
        -DRVFORGE_RUN_TESTS=ON \
        -G Ninja \
        2>&1 | tee "${ROOT}/docs/errors/_configure.log"

    log_info "CMake configure done"
}

build_libraries() {
    log_step "Building GMP..."
    cmake --build "${BUILD_DIR}" --target gmp_riscv -- -j${NPROC} \
        2>&1 | tee "${ROOT}/docs/errors/_gmp_build.log"
    log_info "GMP done"

    log_step "Building MPFR..."
    cmake --build "${BUILD_DIR}" --target mpfr_riscv -- -j${NPROC} \
        2>&1 | tee "${ROOT}/docs/errors/_mpfr_build.log"
    log_info "MPFR done"

    log_step "Building OpenBLAS..."
    cmake --build "${BUILD_DIR}" --target openblas_riscv -- -j${NPROC} \
        2>&1 | tee "${ROOT}/docs/errors/_openblas_build.log"
    log_info "OpenBLAS done"
}

build_harness() {
    log_step "Building smoke test binaries..."
    cmake --build "${BUILD_DIR}" --target mpfr_smoke blas_smoke -- -j${NPROC} \
        2>&1 | tee "${ROOT}/docs/errors/_harness_build.log"
    log_info "Harness binaries built"
}

run_qemu_check() {
    log_step "Running QEMU execution check..."
    chmod +x "${ROOT}/scripts/run_qemu.sh"
    "${ROOT}/scripts/run_qemu.sh"
}

record_build_artifacts() {
    local out="${ROOT}/docs/errors/_build_log.md"
    log_step "Recording build artifacts..."
    {
        echo "# Build Record"
        echo "Date: $(date -u)"
        echo ""
        echo "## ELF Headers"
        echo '```'
        riscv64-linux-gnu-readelf -h "${BUILD_DIR}/bin/mpfr_smoke" \
            | grep -E "Class|Machine|Type|Entry"
        echo "---"
        riscv64-linux-gnu-readelf -h "${BUILD_DIR}/bin/blas_smoke" \
            | grep -E "Class|Machine|Type|Entry"
        echo '```'
        echo ""
        echo "## Binary Sizes"
        echo '```'
        riscv64-linux-gnu-size "${BUILD_DIR}/bin/mpfr_smoke"
        riscv64-linux-gnu-size "${BUILD_DIR}/bin/blas_smoke"
        echo '```'
        echo ""
        echo "## Installed Libraries"
        echo '```'
        find "${BUILD_DIR}/install" -name "*.a" -exec ls -lh {} \;
        echo '```'
    } > "${out}"
    cat "${out}"
}

main() {
    log_info "=== RVForge Bootstrap ==="
    mkdir -p "${ROOT}/docs/errors"

    case "${1:-}" in
        --install-deps)
            install_deps
            verify_toolchain
            ;;
        --configure)
            verify_toolchain
            configure_build
            ;;
        --build)
            verify_toolchain
            configure_build
            build_libraries
            build_harness
            ;;
        --run)
            run_qemu_check
            ;;
        --record)
            record_build_artifacts
            ;;
        --all|"")
            verify_toolchain
            configure_build
            build_libraries
            build_harness
            run_qemu_check
            record_build_artifacts
            ;;
        *)
            echo "Usage: $0 [--install-deps|--configure|--build|--run|--record|--all]"
            exit 1
            ;;
    esac

    log_info "=== Complete ==="
}

main "$@"

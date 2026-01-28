#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/build"
BIN_DIR="${BUILD_DIR}/bin"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[QEMU]${NC}   $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}   $*"; }
log_error() { echo -e "${RED}[ERROR]${NC}  $*"; }
log_step()  { echo -e "${CYAN}[RUN]${NC}    $*"; }

QEMU_BIN="qemu-riscv64-static"
QEMU_LD="/usr/riscv64-linux-gnu"
QEMU_FLAGS="-L ${QEMU_LD}"

RESULT_LOG="${ROOT}/docs/errors/qemu_run.log"
mkdir -p "${ROOT}/docs/errors"
: > "${RESULT_LOG}"

check_qemu() {
    if ! command -v "${QEMU_BIN}" &>/dev/null; then
        log_error "${QEMU_BIN} not found. Install: sudo apt install qemu-user-static"
        exit 1
    fi
    log_info "QEMU: $(${QEMU_BIN} --version | head -1)"
}

check_binary() {
    local bin="$1"
    if [ ! -f "${bin}" ]; then
        log_error "Binary not found: ${bin}"
        return 1
    fi

    local elf_class
    elf_class=$(riscv64-linux-gnu-readelf -h "${bin}" 2>/dev/null \
        | grep "Class:" | awk '{print $2}')
    local elf_machine
    elf_machine=$(riscv64-linux-gnu-readelf -h "${bin}" 2>/dev/null \
        | grep "Machine:" | awk '{$1=""; print}' | xargs)

    if [[ "${elf_class}" == "ELF64" && "${elf_machine}" == *"RISC-V"* ]]; then
        log_info "$(basename ${bin}): ELF64 RISC-V ✓"
        return 0
    else
        log_error "$(basename ${bin}): unexpected ELF class=${elf_class} machine=${elf_machine}"
        return 1
    fi
}

run_binary() {
    local bin="$1"
    local label="$2"
    local rc=0

    log_step "Executing ${label} under ${QEMU_BIN}..."
    echo "=== ${label} ===" >> "${RESULT_LOG}"

    set +e
    ${QEMU_BIN} ${QEMU_FLAGS} "${bin}" 2>&1 | tee -a "${RESULT_LOG}"
    rc=$?
    set -e

    echo "" >> "${RESULT_LOG}"

    if [ $rc -eq 0 ]; then
        log_info "${label}: EXIT 0 ✓"
    else
        log_error "${label}: EXIT ${rc} ✗"
    fi
    return $rc
}

check_no_segfault() {
    local log="${RESULT_LOG}"
    if grep -qiE "segmentation fault|illegal instruction|bus error|SIGILL|SIGSEGV" "${log}"; then
        log_error "Fatal signal detected in QEMU output"
        grep -iE "segmentation fault|illegal instruction|bus error|SIGILL|SIGSEGV" "${log}"
        return 1
    fi
    log_info "No fatal signals detected"
    return 0
}

print_summary() {
    local mpfr_rc=$1
    local blas_rc=$2

    echo ""
    echo "╔══════════════════════════════════════╗"
    echo "║       QEMU Execution Summary          ║"
    echo "╠══════════════════════════════════════╣"
    printf "║  %-20s  %s  ║\n" "mpfr_smoke" \
        "$([ $mpfr_rc -eq 0 ] && echo '✅ PASS' || echo '❌ FAIL')"
    printf "║  %-20s  %s  ║\n" "blas_smoke" \
        "$([ $blas_rc -eq 0 ] && echo '✅ PASS' || echo '❌ FAIL')"
    echo "╚══════════════════════════════════════╝"
    echo ""
    log_info "Full output: ${RESULT_LOG}"
}

main() {
    log_info "=== RVForge QEMU Execution Runner ==="
    check_qemu

    local mpfr_bin="${BIN_DIR}/mpfr_smoke"
    local blas_bin="${BIN_DIR}/blas_smoke"
    local overall_rc=0
    local mpfr_rc=0
    local blas_rc=0

    check_binary "${mpfr_bin}" || { log_error "Build MPFR first: cmake --build build --target mpfr_smoke"; exit 1; }
    check_binary "${blas_bin}" || { log_error "Build BLAS first: cmake --build build --target blas_smoke"; exit 1; }

    run_binary "${mpfr_bin}" "mpfr_smoke" || mpfr_rc=$?
    run_binary "${blas_bin}" "blas_smoke" || blas_rc=$?

    check_no_segfault || overall_rc=1

    print_summary $mpfr_rc $blas_rc

    [ $mpfr_rc -ne 0 ] && overall_rc=1
    [ $blas_rc -ne 0 ] && overall_rc=1

    exit $overall_rc
}

main "$@"

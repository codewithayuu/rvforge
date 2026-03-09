#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/build"
BIN_DIR="${BUILD_DIR}/bin"
RESULT_LOG="${ROOT}/audit/results/qemu_run_.log"

mkdir -p "${ROOT}/audit/results"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[QEMU]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }
log_step()  { echo -e "${CYAN}[RUN]${NC}   $*"; }

QEMU_BIN="qemu-riscv64-static"
QEMU_LD="/usr/riscv64-linux-gnu"

: > "${RESULT_LOG}"

TESTS=(
    mpfr_smoke
    blas_smoke
    numerical_suite
    simd_audit_report
)

declare -A TEST_RC

for t in "${TESTS[@]}"; do
    TEST_RC[$t]=0
done

run_test() {
    local name="$1"
    local bin="${BIN_DIR}/${name}"
    local rc=0

    if [ ! -f "${bin}" ]; then
        log_error "${name}: binary not found at ${bin}"
        TEST_RC[$name]=2
        return
    fi

    local elf_machine
    elf_machine=$(riscv64-linux-gnu-readelf -h "${bin}" 2>/dev/null \
        | grep "Machine:" | awk '{$1=""; print}' | xargs)

    if [[ "${elf_machine}" != *"RISC-V"* ]]; then
        log_error "${name}: not a RISC-V ELF (machine=${elf_machine})"
        TEST_RC[$name]=3
        return
    fi

    log_step "Running ${name}..."
    echo "=== ${name} ===" >> "${RESULT_LOG}"

    set +e
    timeout 300 ${QEMU_BIN} -L ${QEMU_LD} "${bin}" \
        2>&1 | tee -a "${RESULT_LOG}"
    rc=$?
    set -e

    echo "" >> "${RESULT_LOG}"
    TEST_RC[$name]=$rc
}

print_summary() {
    echo ""
    echo "╔══════════════════════════════════════════════════════╗"
    echo "║            QEMU Test Summary                  ║"
    echo "╠══════════════════════════════════════════════════════╣"

    local overall=0
    for t in "${TESTS[@]}"; do
        local rc=${TEST_RC[$t]}
        local status
        case $rc in
            0) status="✅ PASS" ;;
            2) status="⚠️  SKIP (not built)" ;;
            3) status="⚠️  SKIP (wrong arch)" ;;
            *) status="❌ FAIL (rc=${rc})" ; overall=1 ;;
        esac
        printf "║  %-28s  %s          ║\n" "${t}" "${status}"
    done

    echo "╚══════════════════════════════════════════════════════╝"
    echo ""
    log_info "Full log: ${RESULT_LOG}"
    return $overall
}

main() {
    log_info "=== QEMU Runner ==="

    for t in "${TESTS[@]}"; do
        run_test "$t"
    done

    print_summary
}

main "$@"

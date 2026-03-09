#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/build"
RESULTS_DIR="${ROOT}/audit/results"

mkdir -p "${RESULTS_DIR}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[DIFF]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

EPSILON="${1:-1e-10}"
QEMU_BIN="qemu-riscv64-static"
RISCV_BIN="${BUILD_DIR}/bin/numerical_suite"

X86_RESULT="${RESULTS_DIR}/x86_64_output.txt"
RISCV_RESULT="${RESULTS_DIR}/riscv64_output.txt"
DIFF_REPORT="${RESULTS_DIR}/diff_report.md"

run_x86_native() {
    log_info "Running native x86_64 numerical_suite..."
    if [ ! -f "${BUILD_DIR}/bin/numerical_suite_x86" ]; then
        log_warn "x86_64 native binary not found"
        log_warn "Build with: cmake -S . -B build_x86 && cmake --build build_x86"
        echo "x86_64 native binary not available" > "${X86_RESULT}"
        return 0
    fi
    "${BUILD_DIR}/bin/numerical_suite_x86" > "${X86_RESULT}" 2>&1
    log_info "x86_64 output: ${X86_RESULT}"
}

run_riscv_qemu() {
    log_info "Running riscv64 numerical_suite under QEMU..."
    if [ ! -f "${RISCV_BIN}" ]; then
        log_error "RISC-V binary not found: ${RISCV_BIN}"
        exit 1
    fi
    ${QEMU_BIN} -L /usr/riscv64-linux-gnu "${RISCV_BIN}" \
        > "${RISCV_RESULT}" 2>&1
    log_info "riscv64 output: ${RISCV_RESULT}"
}

extract_numeric_results() {
    local file="$1"
    grep -E "err=[0-9e.+-]+" "${file}" \
        | sed 's/.*err=\([0-9e.+-]*\).*/\1/' \
        2>/dev/null || true
}

extract_status_lines() {
    local file="$1"
    grep -E "OK|FAIL|PASS" "${file}" 2>/dev/null || true
}

compare_results() {
    log_info "Comparing outputs..."

    {
        echo "# RVForge Output Diff"
        echo ""
        echo "Epsilon threshold: \`${EPSILON}\`"
        echo ""
        echo "## riscv64 (QEMU) vs x86_64 (native)"
        echo ""
        echo "\`\`\`"
        echo "=== riscv64 output ==="
        cat "${RISCV_RESULT}"
        echo ""
        echo "=== x86_64 output ==="
        cat "${X86_RESULT}"
        echo "\`\`\`"
        echo ""
        echo "## Test Status Comparison"
        echo ""
        echo "| Test | x86_64 | riscv64 |"
        echo "|------|--------|---------|"
    } > "${DIFF_REPORT}"

    while IFS= read -r line; do
        testname=$(echo "$line" | grep -oE '[A-Z_]+' | head -1)
        riscv_line=$(grep "${testname}" "${RISCV_RESULT}" 2>/dev/null | head -1 || echo "NOT FOUND")
        x86_status=$(echo "$line"       | grep -oE "OK|FAIL|PASS" | head -1 || echo "?")
        riscv_status=$(echo "$riscv_line" | grep -oE "OK|FAIL|PASS" | head -1 || echo "?")
        echo "| \`${testname}\` | ${x86_status} | ${riscv_status} |" >> "${DIFF_REPORT}"
    done < <(extract_status_lines "${X86_RESULT}")

    echo "" >> "${DIFF_REPORT}"
    echo "## Numeric Error Values (riscv64)" >> "${DIFF_REPORT}"
    echo "" >> "${DIFF_REPORT}"
    echo "\`\`\`" >> "${DIFF_REPORT}"
    grep -E "err=" "${RISCV_RESULT}" >> "${DIFF_REPORT}" 2>/dev/null || true
    echo "\`\`\`" >> "${DIFF_REPORT}"

    log_info "Diff report: ${DIFF_REPORT}"
    cat "${DIFF_REPORT}"
}

check_riscv_pass() {
    if grep -q "STATUS: FAIL" "${RISCV_RESULT}"; then
        log_error "riscv64 numerical_suite: STATUS FAIL"
        return 1
    fi
    if grep -q "STATUS: PASS" "${RISCV_RESULT}"; then
        log_info "riscv64 numerical_suite: STATUS PASS ✓"
        return 0
    fi
    log_warn "STATUS line not found in riscv64 output"
    return 1
}

main() {
    log_info "=== RVForge Result Diff ==="
    log_info "Epsilon: ${EPSILON}"

    run_x86_native
    run_riscv_qemu
    compare_results
    check_riscv_pass

    log_info "=== Diff Complete ==="
}

main "$@"

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/build"
BIN_DIR="${BUILD_DIR}/bin"
RESULTS_DIR="${ROOT}/validation/results"
X86_DIR="${RESULTS_DIR}/x86_64"
RISCV_DIR="${RESULTS_DIR}/riscv64"
REPORT_DIR="${ROOT}/docs"

mkdir -p "${X86_DIR}" "${RISCV_DIR}" "${REPORT_DIR}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[VALIDATE]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}     $*"; }
log_error() { echo -e "${RED}[ERROR]${NC}    $*"; }
log_step()  { echo -e "${CYAN}[STEP]${NC}     $*"; }
log_sep()   { echo -e "${BOLD}$(printf '─%.0s' {1..65})${NC}"; }

EPSILON="${EPSILON:-1e-10}"
EPSILON_FLOAT="${EPSILON_FLOAT:-1e-5}"
QEMU_BIN="qemu-riscv64-static"
QEMU_LD="/usr/riscv64-linux-gnu"
QEMU_TIMEOUT=600

declare -A BINARY_RC
OVERALL_RC=0

check_prerequisites() {
    log_step "Checking prerequisites..."

    local missing=0
    for tool in "${QEMU_BIN}" riscv64-linux-gnu-readelf python3; do
        if ! command -v "${tool}" &>/dev/null; then
            log_error "Missing: ${tool}"
            missing=1
        else
            log_info "  ✓ ${tool}"
        fi
    done

    [ $missing -eq 0 ] || { log_error "Install missing tools and retry"; exit 1; }

    if [ ! -d "${BIN_DIR}" ]; then
        log_error "Binary dir not found: ${BIN_DIR}"
        log_error "Run: cmake --build build -- -j\$(nproc)"
        exit 1
    fi
}

verify_riscv_elf() {
    local bin="$1"
    local name
    name=$(basename "${bin}")

    if [ ! -f "${bin}" ]; then
        log_warn "${name}: not built, skipping"
        return 1
    fi

    local machine
    machine=$(riscv64-linux-gnu-readelf -h "${bin}" 2>/dev/null \
        | grep "Machine:" | awk '{$1=""; print}' | xargs)

    if [[ "${machine}" != *"RISC-V"* ]]; then
        log_error "${name}: ELF machine='${machine}' — not RISC-V"
        return 1
    fi

    local class
    class=$(riscv64-linux-gnu-readelf -h "${bin}" 2>/dev/null \
        | grep "Class:" | awk '{print $2}')

    if [[ "${class}" != "ELF64" ]]; then
        log_error "${name}: ELF class='${class}' — not 64-bit"
        return 1
    fi

    log_info "  ${name}: ELF64 RISC-V ✓"
    return 0
}

run_native_x86() {
    local name="$1"
    local x86_bin="${BUILD_DIR}/bin_x86/${name}"
    local out_file="${X86_DIR}/${name}.txt"

    if [ ! -f "${x86_bin}" ]; then
        log_warn "${name}: x86_64 binary not found at ${x86_bin}"
        echo "x86_64_binary_not_available" > "${out_file}"
        return 0
    fi

    log_step "Running ${name} (x86_64 native)..."
    set +e
    timeout 120 "${x86_bin}" > "${out_file}" 2>&1
    local rc=$?
    set -e

    if [ $rc -ne 0 ]; then
        log_warn "${name}: x86_64 exited ${rc}"
    fi
    log_info "  x86_64 output: ${out_file}"
}

run_qemu_riscv() {
    local name="$1"
    local riscv_bin="${BIN_DIR}/${name}"
    local out_file="${RISCV_DIR}/${name}.txt"
    local rc=0

    log_step "Running ${name} under QEMU..."

    set +e
    timeout ${QEMU_TIMEOUT} \
        ${QEMU_BIN} -L ${QEMU_LD} "${riscv_bin}" \
        > "${out_file}" 2>&1
    rc=$?
    set -e

    BINARY_RC["${name}"]=$rc

    if [ $rc -eq 0 ]; then
        log_info "  ${name}: QEMU exit 0 ✓"
    elif [ $rc -eq 124 ]; then
        log_error "  ${name}: QEMU TIMEOUT after ${QEMU_TIMEOUT}s"
        OVERALL_RC=1
    else
        log_error "  ${name}: QEMU exit ${rc}"
        OVERALL_RC=1
    fi

    cat "${out_file}"
    echo ""
}

check_status_line() {
    local name="$1"
    local out_file="${RISCV_DIR}/${name}.txt"

    if grep -q "STATUS: PASS" "${out_file}" 2>/dev/null; then
        log_info "  ${name}: STATUS PASS ✓"
        return 0
    elif grep -q "STATUS: FAIL" "${out_file}" 2>/dev/null; then
        log_error "  ${name}: STATUS FAIL"
        OVERALL_RC=1
        return 1
    else
        log_warn "  ${name}: no STATUS line found"
        return 0
    fi
}

check_no_crash() {
    local name="$1"
    local out_file="${RISCV_DIR}/${name}.txt"

    if grep -qiE "segmentation fault|illegal instruction|bus error|SIGILL|SIGSEGV|SIGBUS|Aborted" \
               "${out_file}" 2>/dev/null; then
        log_error "  ${name}: crash signal detected"
        grep -iE "segmentation fault|illegal instruction|bus error|SIGILL|SIGSEGV" \
            "${out_file}" | head -3
        OVERALL_RC=1
        return 1
    fi
    return 0
}

run_epsilon_diff() {
    local name="$1"
    local x86_out="${X86_DIR}/${name}.txt"
    local rv_out="${RISCV_DIR}/${name}.txt"
    local md_out="${REPORT_DIR}/${name}_diff.md"
    local json_out="${REPORT_DIR}/${name}_diff.json"

    if [ ! -f "${x86_out}" ] || grep -q "not_available" "${x86_out}" 2>/dev/null; then
        log_warn "  ${name}: no x86_64 reference — skipping epsilon diff"
        return 0
    fi

    log_step "Epsilon diff: ${name} (eps=${EPSILON})..."

    set +e
    python3 "${ROOT}/validation/compare_outputs.py" \
        "${x86_out}" \
        "${rv_out}" \
        --epsilon "${EPSILON}" \
        --ulp-limit 4 \
        --output-md  "${md_out}" \
        --output-json "${json_out}" \
        --strict
    local diff_rc=$?
    set -e

    if [ $diff_rc -eq 0 ]; then
        log_info "  ${name}: epsilon diff PASS ✓"
    else
        log_error "  ${name}: epsilon diff FAIL — values exceed eps=${EPSILON}"
        OVERALL_RC=1
    fi
}

audit_binary_symbols() {
    local name="$1"
    local bin="${BIN_DIR}/${name}"

    log_step "Symbol audit: ${name}..."

    local x86_syms
    x86_syms=$(riscv64-linux-gnu-objdump -d "${bin}" 2>/dev/null \
        | grep -cE "xmm[0-9]|ymm[0-9]|_mm_|__SSE|__AVX" || true)

    if [ "${x86_syms}" -gt 0 ]; then
        log_error "  ${name}: ${x86_syms} x86 SIMD references found in riscv64 binary"
        OVERALL_RC=1
    else
        log_info "  ${name}: no x86 SIMD symbols ✓"
    fi
}

generate_validation_report() {
    local report="${ROOT}/docs/validation_report.md"
    log_step "Generating validation report..."

    {
        echo "# RVForge Validation Report"
        echo ""
        echo "Generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo ""
        echo "## Environment"
        echo ""
        echo '```'
        echo "Host:   $(uname -m) $(uname -r)"
        echo "GCC:    $(riscv64-linux-gnu-gcc --version | head -1)"
        echo "QEMU:   $(${QEMU_BIN} --version | head -1)"
        echo "Python: $(python3 --version)"
        echo '```'
        echo ""
        echo "## Binary Verification"
        echo ""
        echo "| Binary | ELF Class | Machine | Status |"
        echo "|--------|-----------|---------|--------|"
        for bin in "${BIN_DIR}"/*; do
            [ -f "${bin}" ] || continue
            local bname
            bname=$(basename "${bin}")
            local class machine
            class=$(riscv64-linux-gnu-readelf -h "${bin}" 2>/dev/null \
                | grep "Class:" | awk '{print $2}' || echo "?")
            machine=$(riscv64-linux-gnu-readelf -h "${bin}" 2>/dev/null \
                | grep "Machine:" | awk '{$1=""; print}' | xargs || echo "?")
            local ok="✅"
            [[ "${class}" != "ELF64" || "${machine}" != *"RISC-V"* ]] && ok="❌"
            echo "| \`${bname}\` | ${class} | ${machine} | ${ok} |"
        done
        echo ""
        echo "## QEMU Execution Results"
        echo ""
        echo "| Binary | Exit Code | STATUS line | Crashes |"
        echo "|--------|-----------|-------------|---------|"
        for name in "${!BINARY_RC[@]}"; do
            local rc=${BINARY_RC[$name]}
            local out="${RISCV_DIR}/${name}.txt"
            local status_line="—"
            local crashes="none"
            grep -q "STATUS: PASS" "${out}" 2>/dev/null && status_line="✅ PASS"
            grep -q "STATUS: FAIL" "${out}" 2>/dev/null && status_line="❌ FAIL"
            grep -qiE "segfault|SIGILL|SIGSEGV" "${out}" 2>/dev/null \
                && crashes="❌ DETECTED"
            echo "| \`${name}\` | ${rc} | ${status_line} | ${crashes} |"
        done
        echo ""
        echo "## Epsilon Diff Results"
        echo ""
        echo "Epsilon: \`${EPSILON}\`"
        echo ""
        for md in "${REPORT_DIR}"/*_diff.md; do
            [ -f "${md}" ] || continue
            echo "### $(basename ${md} _diff.md)"
            cat "${md}"
            echo ""
        done
        echo ""
        echo "## Overall Result"
        echo ""
        if [ ${OVERALL_RC} -eq 0 ]; then
            echo "### ✅ ALL VALIDATION CHECKS PASSED"
        else
            echo "### ❌ VALIDATION FAILED — see details above"
        fi
    } > "${report}"

    log_info "Report: ${report}"
    cat "${report}"
}

SUITE_BINARIES=(
    mpfr_smoke
    blas_smoke
    mpfr_full_suite
    blas_full_suite
    numerical_suite
    simd_audit_report
)

main() {
    log_sep
    log_info "RVForge QEMU Validation Pipeline"
    log_info "Epsilon:  ${EPSILON}"
    log_info "QEMU bin: $(command -v ${QEMU_BIN})"
    log_sep

    check_prerequisites

    log_sep
    log_step "ELF Verification"
    log_sep
    for name in "${SUITE_BINARIES[@]}"; do
        verify_riscv_elf "${BIN_DIR}/${name}" || true
    done

    log_sep
    log_step "Symbol Audit (no x86 SIMD in riscv64 binaries)"
    log_sep
    for name in "${SUITE_BINARIES[@]}"; do
        [ -f "${BIN_DIR}/${name}" ] && audit_binary_symbols "${name}" || true
    done

    log_sep
    log_step "x86_64 Native Baseline"
    log_sep
    for name in "${SUITE_BINARIES[@]}"; do
        run_native_x86 "${name}"
    done

    log_sep
    log_step "QEMU riscv64 Execution"
    log_sep
    for name in "${SUITE_BINARIES[@]}"; do
        if [ -f "${BIN_DIR}/${name}" ]; then
            run_qemu_riscv "${name}"
            check_status_line "${name}"
            check_no_crash "${name}"
        fi
    done

    log_sep
    log_step "Epsilon Diff"
    log_sep
    for name in "${SUITE_BINARIES[@]}"; do
        [ -f "${RISCV_DIR}/${name}.txt" ] && run_epsilon_diff "${name}" || true
    done

    log_sep
    log_step "Validation Report"
    log_sep
    generate_validation_report

    log_sep
    if [ ${OVERALL_RC} -eq 0 ]; then
        log_info "✅ ALL CHECKS PASSED"
    else
        log_error "❌ VALIDATION FAILED"
    fi
    log_sep

    exit ${OVERALL_RC}
}

main "$@"

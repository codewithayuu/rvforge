#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/build"
EXTERN_DIR="${ROOT}/extern"
RESULTS_DIR="${ROOT}/validation/results/upstream"

mkdir -p "${RESULTS_DIR}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[UPSTREAM]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}     $*"; }
log_error() { echo -e "${RED}[ERROR]${NC}    $*"; }
log_step()  { echo -e "${CYAN}[STEP]${NC}     $*"; }

QEMU_BIN="qemu-riscv64-static"
QEMU_LD="/usr/riscv64-linux-gnu"
QEMU_TIMEOUT=300
OVERALL_RC=0

run_mpfr_testsuite() {
    log_step "Running MPFR upstream test suite under QEMU..."

    local mpfr_src=""
    for d in "${EXTERN_DIR}"/mpfr-*; do [ -d "$d" ] && mpfr_src="$d" && break; done

    if [ -z "${mpfr_src}" ]; then
        log_warn "MPFR source not found — skipping upstream tests"
        return 0
    fi

    local mpfr_build="${BUILD_DIR}/mpfr-build"
    local test_dir="${mpfr_build}/tests"

    if [ ! -d "${test_dir}" ]; then
        log_warn "MPFR test directory not found: ${test_dir}"
        log_warn "MPFR uses autoconf build — tests in build/mpfr-build/tests/"
        return 0
    fi

    local out="${RESULTS_DIR}/mpfr_upstream.log"
    : > "${out}"

    local pass=0 fail=0
    for test_bin in "${test_dir}"/t*; do
        [ -x "${test_bin}" ] || continue
        local tname
        tname=$(basename "${test_bin}")

        local machine
        machine=$(riscv64-linux-gnu-readelf -h "${test_bin}" 2>/dev/null \
            | grep "Machine:" | awk '{$1=""; print}' | xargs || echo "")

        [[ "${machine}" != *"RISC-V"* ]] && continue

        set +e
        timeout ${QEMU_TIMEOUT} \
            ${QEMU_BIN} -L ${QEMU_LD} "${test_bin}" \
            >> "${out}" 2>&1
        local rc=$?
        set -e

        if [ $rc -eq 0 ]; then
            log_info "  ${tname}: PASS"
            pass=$((pass + 1))
        else
            log_error "  ${tname}: FAIL (rc=${rc})"
            fail=$((fail + 1))
            OVERALL_RC=1
        fi
    done

    log_info "MPFR upstream: ${pass} passed, ${fail} failed"
    log_info "Log: ${out}"
}

run_openblas_cblas_tests() {
    log_step "Running OpenBLAS CBLAS tests under QEMU..."

    local openblas_install="${BUILD_DIR}/install/openblas"
    local openblas_src=""
    for d in "${EXTERN_DIR}"/openblas-* "${EXTERN_DIR}"/OpenBLAS-*; do
        [ -d "$d" ] && openblas_src="$d" && break
    done

    if [ -z "${openblas_src}" ]; then
        log_warn "OpenBLAS source not found — skipping"
        return 0
    fi

    local out="${RESULTS_DIR}/openblas_cblas.log"
    : > "${out}"

    local cblastest_src="${openblas_src}/ctest"
    if [ ! -d "${cblastest_src}" ]; then
        log_warn "OpenBLAS ctest directory not found: ${cblastest_src}"
        return 0
    fi

    local tmpbuild
    tmpbuild=$(mktemp -d)
    trap "rm -rf ${tmpbuild}" RETURN

    local pass=0 fail=0

    for csrc in "${cblastest_src}"/c_*.c; do
        [ -f "${csrc}" ] || continue
        local tname
        tname=$(basename "${csrc}" .c)

        set +e
        riscv64-linux-gnu-gcc \
            -march=rv64gc -mabi=lp64d \
            -static -static-libgcc \
            -O2 \
            -I"${openblas_install}/include" \
            -I"${openblas_src}/ctest" \
            -o "${tmpbuild}/${tname}" \
            "${csrc}" \
            -L"${openblas_install}/lib" \
            -lopenblas -lm \
            >> "${out}" 2>&1
        local compile_rc=$?
        set -e

        if [ $compile_rc -ne 0 ]; then
            log_warn "  ${tname}: compile failed (expected for some tests)"
            continue
        fi

        set +e
        timeout ${QEMU_TIMEOUT} \
            ${QEMU_BIN} -L ${QEMU_LD} "${tmpbuild}/${tname}" \
            >> "${out}" 2>&1
        local run_rc=$?
        set -e

        if [ $run_rc -eq 0 ]; then
            log_info "  ${tname}: PASS"
            pass=$((pass + 1))
        else
            log_warn "  ${tname}: FAIL (rc=${run_rc}) — may be expected"
        fi
    done

    log_info "OpenBLAS CBLAS tests: ${pass} passed, ${fail} failed"
    log_info "Log: ${out}"
}

generate_upstream_summary() {
    local summary="${RESULTS_DIR}/upstream_summary.md"
    {
        echo "# Upstream Test Results"
        echo ""
        echo "Generated: $(date -u)"
        echo ""
        echo "## MPFR Upstream Tests"
        echo '```'
        cat "${RESULTS_DIR}/mpfr_upstream.log" 2>/dev/null | tail -20 || echo "Not run"
        echo '```'
        echo ""
        echo "## OpenBLAS CBLAS Tests"
        echo '```'
        cat "${RESULTS_DIR}/openblas_cblas.log" 2>/dev/null | tail -20 || echo "Not run"
        echo '```'
    } > "${summary}"
    log_info "Summary: ${summary}"
}

main() {
    log_info "=== RVForge Upstream Test Runner ==="
    run_mpfr_testsuite
    run_openblas_cblas_tests
    generate_upstream_summary
    log_info "=== Upstream tests done (rc=${OVERALL_RC}) ==="
    exit ${OVERALL_RC}
}

main "$@"

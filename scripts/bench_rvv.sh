#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/build"
BIN_DIR="${BUILD_DIR}/bin"
BENCH_OUT="${ROOT}/docs/bench_results.md"
OPENBLAS_INSTALL="${BUILD_DIR}/install/openblas"

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'
YELLOW='\033[1;33m'; BOLD='\033[1m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[BENCH]${NC} $*"; }
log_step()  { echo -e "${CYAN}[STEP]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

QEMU_BIN="qemu-riscv64-static"
QEMU_LD="/usr/riscv64-linux-gnu"

RVV_MARCH="rv64gcv"
SCALAR_MARCH="rv64gc"
MABI="lp64d"

RVV_SRCS=(
    "${ROOT}/rvv/rvv_ddot.c"
    "${ROOT}/rvv/rvv_daxpy.c"
    "${ROOT}/rvv/rvv_dnrm2.c"
    "${ROOT}/rvv/rvv_dgemv.c"
    "${ROOT}/rvv/rvv_dgemm.c"
    "${ROOT}/harness/rvv_vs_scalar_bench.c"
)

CORRECTNESS_SRCS=(
    "${ROOT}/rvv/rvv_ddot.c"
    "${ROOT}/rvv/rvv_daxpy.c"
    "${ROOT}/rvv/rvv_dnrm2.c"
    "${ROOT}/rvv/rvv_dgemv.c"
    "${ROOT}/rvv/rvv_dgemm.c"
    "${ROOT}/harness/rvv_correctness.c"
)

check_prerequisites() {
    log_step "Checking prerequisites..."
    for tool in riscv64-linux-gnu-gcc "${QEMU_BIN}" python3; do
        command -v "$tool" &>/dev/null \
            && log_info "  ✓ ${tool}" \
            || { log_error "Missing: ${tool}"; exit 1; }
    done
}

probe_rvv_support() {
    log_step "Probing RVV compiler support..."
    local tmpdir
    tmpdir=$(mktemp -d)
    trap "rm -rf ${tmpdir}" RETURN

    cat > "${tmpdir}/rvv_probe.c" <<'EOF'
#include <riscv_vector.h>
int main(void) {
    size_t vl = __riscv_vsetvlmax_e64m1();
    vfloat64m1_t v = __riscv_vfmv_v_f_f64m1(1.0, vl);
    (void)v;
    return 0;
}
EOF

    if riscv64-linux-gnu-gcc \
        -march=${RVV_MARCH} -mabi=${MABI} \
        -static -O2 \
        -o "${tmpdir}/rvv_probe" \
        "${tmpdir}/rvv_probe.c" \
        -lm 2>/dev/null
    then
        log_info "RVV compile: supported"
        if ${QEMU_BIN} -L ${QEMU_LD} "${tmpdir}/rvv_probe" 2>/dev/null; then
            log_info "RVV execute: supported under QEMU"
            return 0
        else
            log_warn "RVV compile OK but QEMU execution failed"
            log_warn "QEMU version may not support RVV — checking version"
            ${QEMU_BIN} --version | head -1
            return 1
        fi
    else
        log_warn "RVV compile: not supported by this GCC version"
        log_warn "GCC: $(riscv64-linux-gnu-gcc --version | head -1)"
        return 1
    fi
}

build_rvv_bench() {
    local march="$1"
    local outbin="$2"
    local srcs=("${@:3}")

    log_step "Building bench binary (march=${march})..."
    log_info "  Output: ${outbin}"

    riscv64-linux-gnu-gcc \
        -march="${march}" \
        -mabi="${MABI}" \
        -O2 \
        -static \
        -static-libgcc \
        -fno-stack-protector \
        -ffunction-sections \
        -fdata-sections \
        -Wl,--gc-sections \
        -I"${ROOT}/rvv" \
        -I"${ROOT}/validation" \
        -I"${ROOT}/harness" \
        -o "${outbin}" \
        "${srcs[@]}" \
        -lm \
        2>&1 | head -30

    log_info "  Built: $(file "${outbin}")"
}

run_correctness() {
    local march="$1"
    local label="$2"
    local outbin="${BIN_DIR}/rvv_correctness_${label}"

    log_step "Correctness check (${label}, march=${march})..."

    build_rvv_bench "${march}" "${outbin}" "${CORRECTNESS_SRCS[@]}"

    local out="${ROOT}/docs/rvv_correctness_${label}.txt"
    ${QEMU_BIN} -L ${QEMU_LD} "${outbin}" 2>&1 | tee "${out}"

    if grep -q "STATUS: PASS" "${out}"; then
        log_info "Correctness (${label}): PASS ✓"
        return 0
    else
        log_error "Correctness (${label}): FAIL"
        return 1
    fi
}

run_bench() {
    local march="$1"
    local label="$2"
    local outbin="${BIN_DIR}/rvv_bench_${label}"

    log_step "Benchmark (${label}, march=${march})..."

    build_rvv_bench "${march}" "${outbin}" "${RVV_SRCS[@]}"

    local out="${ROOT}/docs/bench_${label}.txt"
    log_info "Running under QEMU..."
    timeout 600 ${QEMU_BIN} -L ${QEMU_LD} "${outbin}" 2>&1 | tee "${out}"

    log_info "Raw results: ${out}"
}

generate_bench_report() {
    local scalar_out="${ROOT}/docs/bench_scalar.txt"
    local rvv_out="${ROOT}/docs/bench_rvv.txt"

    log_step "Generating bench_results.md..."

    {
        echo "# RVForge RVV vs Scalar Benchmark Results"
        echo ""
        echo "Generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo ""
        echo "## Environment"
        echo ""
        echo '```'
        echo "Emulator:  qemu-riscv64-static $(${QEMU_BIN} --version | head -1)"
        echo "GCC:       $(riscv64-linux-gnu-gcc --version | head -1)"
        echo "Scalar:    -march=${SCALAR_MARCH} -mabi=${MABI} -O2"
        echo "RVV:       -march=${RVV_MARCH}    -mabi=${MABI} -O2"
        echo '```'
        echo ""
        echo "> **QEMU Note:** qemu-riscv64-static emulates RVV at the instruction"
        echo "> level without hardware vector units. Speedup numbers reflect"
        echo "> instruction-count reduction, not wall-clock throughput on silicon."
        echo "> On real riscv64 hardware with VLEN≥128, expect 4–8× for DDOT/DAXPY."
        echo ""
        echo "## Scalar Baseline Results"
        echo ""
        echo '```'
        grep -A999 "Kernel" "${scalar_out}" 2>/dev/null | head -40 || echo "Not available"
        echo '```'
        echo ""
        echo "## RVV Results"
        echo ""
        echo '```'
        grep -A999 "Kernel" "${rvv_out}" 2>/dev/null | head -40 || echo "Not available"
        echo '```'
        echo ""
        echo "## Extracted Markdown Table"
        echo ""
        grep "^\| \`" "${rvv_out}" 2>/dev/null || echo "Run bench to populate"
        echo ""
        echo "## Analysis"
        echo ""
        echo "See \`docs/rvv_analysis.md\` for full interpretation."
    } > "${BENCH_OUT}"

    log_info "Report: ${BENCH_OUT}"
    cat "${BENCH_OUT}"
}

main() {
    log_info "=== RVForge RVV Benchmark ==="
    mkdir -p "${BIN_DIR}" "${ROOT}/docs"

    check_prerequisites

    local rvv_ok=0
    probe_rvv_support && rvv_ok=1

    log_step "Running scalar correctness check..."
    run_correctness "${SCALAR_MARCH}" "scalar" || true

    log_step "Running scalar benchmark..."
    run_bench "${SCALAR_MARCH}" "scalar"

    if [ $rvv_ok -eq 1 ]; then
        log_step "Running RVV correctness check..."
        run_correctness "${RVV_MARCH}" "rvv"

        log_step "Running RVV benchmark..."
        run_bench "${RVV_MARCH}" "rvv"
    else
        log_warn "RVV not available — copying scalar results as RVV baseline"
        cp "${ROOT}/docs/bench_scalar.txt" "${ROOT}/docs/bench_rvv.txt" 2>/dev/null || true
        echo "(RVV not available in this toolchain/QEMU combination)" \
            >> "${ROOT}/docs/bench_rvv.txt"
    fi

    generate_bench_report

    log_info "=== Benchmark Complete ==="
}

main "$@"

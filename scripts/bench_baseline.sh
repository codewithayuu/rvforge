#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT}/build"
BIN_DIR="${BUILD_DIR}/bin"
BENCH_DIR="${ROOT}/validation/results/bench"

mkdir -p "${BENCH_DIR}"

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'

log_info() { echo -e "${GREEN}[BENCH]${NC} $*"; }
log_step() { echo -e "${CYAN}[STEP]${NC}  $*"; }

QEMU_BIN="qemu-riscv64-static"
QEMU_LD="/usr/riscv64-linux-gnu"

BENCH_SRC="${ROOT}/harness/bench_runner.c"
BENCH_BIN="${BIN_DIR}/bench_runner"
OPENBLAS_INSTALL="${BUILD_DIR}/install/openblas"

build_bench() {
    log_step "Building bench_runner for riscv64..."
    riscv64-linux-gnu-gcc \
        -march=rv64gc -mabi=lp64d \
        -O2 -static -static-libgcc \
        -fno-stack-protector \
        -I"${OPENBLAS_INSTALL}/include" \
        -o "${BENCH_BIN}" \
        "${BENCH_SRC}" \
        -L"${OPENBLAS_INSTALL}/lib" \
        -lopenblas -lm
    log_info "Built: ${BENCH_BIN}"
}

run_bench_qemu() {
    local out="${BENCH_DIR}/riscv64_qemu_bench.txt"
    log_step "Running bench under QEMU..."
    ${QEMU_BIN} -L ${QEMU_LD} "${BENCH_BIN}" > "${out}" 2>&1
    cat "${out}"
    log_info "Results: ${out}"
}

record_sizes() {
    local out="${BENCH_DIR}/binary_sizes.txt"
    log_step "Recording binary sizes..."
    {
        echo "=== Binary Sizes (riscv64 static) ==="
        echo ""
        for bin in "${BIN_DIR}"/*; do
            [ -f "${bin}" ] || continue
            riscv64-linux-gnu-size "${bin}" 2>/dev/null || true
        done
    } > "${out}"
    cat "${out}"
}

generate_bench_report() {
    local report="${BENCH_DIR}/baseline_report.md"
    {
        echo "# RVForge Baseline Performance Report"
        echo ""
        echo "Generated: $(date -u)"
        echo ""
        echo "## Note on QEMU Performance"
        echo ""
        echo "All benchmarks run under \`qemu-riscv64-static\` user-mode emulation."
        echo "QEMU does not JIT RISC-V to x86 at instruction-level granularity for"
        echo "floating-point — expect 10–50× overhead vs native riscv64 hardware."
        echo "These numbers establish a correctness + reproducibility baseline."
        echo "will record RVV vs scalar numbers on the same QEMU version."
        echo ""
        echo "## Binary Sizes"
        echo ""
        echo '```'
        cat "${BENCH_DIR}/binary_sizes.txt" 2>/dev/null || echo "Not generated"
        echo '```'
        echo ""
        echo "## QEMU Execution Timing"
        echo ""
        echo '```'
        cat "${BENCH_DIR}/riscv64_qemu_bench.txt" 2>/dev/null || echo "Not generated"
        echo '```'
    } > "${report}"
    log_info "Report: ${report}"
    cat "${report}"
}

main() {
    log_info "=== RVForge Baseline Benchmark ==="
    [ -f "${BENCH_SRC}" ] && build_bench || log_info "bench_runner.c not found — skipping build"
    [ -f "${BENCH_BIN}" ] && run_bench_qemu || log_info "bench_runner binary not found — skipping run"
    record_sizes
    generate_bench_report
    log_info "=== Benchmark complete ==="
}

main "$@"

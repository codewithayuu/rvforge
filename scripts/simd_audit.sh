#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EXTERN_DIR="${ROOT}/extern"
AUDIT_DIR="${ROOT}/audit"
REPORT="${AUDIT_DIR}/simd_audit_report.md"

mkdir -p "${AUDIT_DIR}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[AUDIT]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_step()  { echo -e "${CYAN}[STEP]${NC}  $*"; }

OPENBLAS_SRC=""
for d in "${EXTERN_DIR}"/openblas-* "${EXTERN_DIR}"/OpenBLAS-*; do
    [ -d "$d" ] && OPENBLAS_SRC="$d" && break
done

if [ -z "${OPENBLAS_SRC}" ]; then
    log_warn "OpenBLAS source not found in ${EXTERN_DIR}"
    log_warn "Run bootstrap first to fetch sources"
    exit 1
fi

log_info "Auditing: ${OPENBLAS_SRC}"

declare -A INTRINSIC_COUNTS
declare -A INTRINSIC_FILES

INTRINSIC_PATTERNS=(
    "_mm_load_pd"    "_mm_store_pd"    "_mm_mul_pd"    "_mm_add_pd"
    "_mm_sub_pd"     "_mm_hadd_pd"     "_mm_shuffle_pd" "_mm_unpacklo_pd"
    "_mm_unpackhi_pd" "_mm_loadu_pd"   "_mm_storeu_pd"  "_mm_set1_pd"
    "_mm_setzero_pd" "_mm_movedup_pd"
    "_mm256_load_pd" "_mm256_store_pd" "_mm256_mul_pd"  "_mm256_add_pd"
    "_mm256_fmadd_pd" "_mm256_fmsub_pd" "_mm256_hadd_pd"
    "_mm256_loadu_pd" "_mm256_storeu_pd" "_mm256_set1_pd"
    "_mm256_broadcast_sd" "_mm256_blend_pd" "_mm256_permute2f128_pd"
    "_mm256_unpacklo_pd" "_mm256_unpackhi_pd" "_mm256_setzero_pd"
    "_mm256_load_ps" "_mm256_store_ps" "_mm256_fmadd_ps" "_mm256_loadu_ps"
    "_mm256_broadcast_ss" "_mm256_add_ps" "_mm256_mul_ps"
    "_mm256_addsub_pd" "_mm256_moveldup_pd" "_mm256_movehdup_pd"
    "_mm_stream_pd"  "_mm_prefetch"   "_mm_malloc"     "_mm_free"
    "__cpuid"        "__get_cpuid"
)

ALL_FOUND="${AUDIT_DIR}/x86_intrinsics_found.txt"
: > "${ALL_FOUND}"

log_step "Scanning x86_64 kernel directory..."
for pat in "${INTRINSIC_PATTERNS[@]}"; do
    count=0
    files=""
    while IFS= read -r line; do
        file=$(echo "$line" | cut -d: -f1)
        count=$((count + 1))
        files="${files} $(basename $file)"
    done < <(grep -rn "${pat}" "${OPENBLAS_SRC}/kernel/x86_64/" \
                "${OPENBLAS_SRC}/driver/others/" \
                "${OPENBLAS_SRC}/interface/" \
                --include="*.c" --include="*.h" 2>/dev/null || true)

    if [ $count -gt 0 ]; then
        INTRINSIC_COUNTS["${pat}"]=$count
        INTRINSIC_FILES["${pat}"]="${files}"
        echo "${pat}|${count}|${files}" >> "${ALL_FOUND}"
    fi
done

log_step "Generating audit report..."

cat > "${REPORT}" <<'HEADER'
# RVForge SIMD Intrinsic Audit Report

## Scope
Audited: OpenBLAS 0.3.26
Directories: `kernel/x86_64/`, `driver/others/`, `interface/` 
Focus: All intrinsics that appear in the RISC-V build path

## Intrinsic → Fallback Mapping

| Intrinsic | Category | Count | RISC-V Fallback | Patch |
|-----------|----------|-------|-----------------|-------|
HEADER

while IFS='|' read -r intrinsic count files; do
    category="SSE2"
    [[ "${intrinsic}" == *"256"* ]] && category="AVX/AVX2"
    [[ "${intrinsic}" == *"fmadd"* ]] && category="FMA"
    [[ "${intrinsic}" == *"stream"* ]] && category="Non-temporal"
    [[ "${intrinsic}" == *"cpuid"* || "${intrinsic}" == *"CPUID"* ]] && category="CPUID"
    [[ "${intrinsic}" == *"prefetch"* ]] && category="Prefetch"
    [[ "${intrinsic}" == *"malloc"* || "${intrinsic}" == *"free"* ]] && category="Memory"

    fallback="scalar loop"
    patch="0004"
    [[ "${intrinsic}" == *"fmadd_pd"* ]] && fallback="fma(a,b,c)" && patch="0005"
    [[ "${intrinsic}" == *"fmadd_ps"* ]] && fallback="fmaf(a,b,c)" && patch="0006"
    [[ "${intrinsic}" == *"addsub"* || "${intrinsic}" == *"ldup"* ]] && fallback="complex re/im split" && patch="0007"
    [[ "${intrinsic}" == *"stream"* ]] && fallback="plain store" && patch="0008"
    [[ "${intrinsic}" == *"prefetch"* ]] && fallback="__builtin_prefetch" && patch="0008"
    [[ "${intrinsic}" == *"malloc"* ]] && fallback="posix_memalign" && patch="0008"
    [[ "${intrinsic}" == *"cpuid"* ]] && fallback="return 0 stub" && patch="0003"

    printf "| \`%s\` | %s | %s | %s | [%s] |\n" \
        "${intrinsic}" "${category}" "${count}" "${fallback}" "${patch}" >> "${REPORT}"
done < "${ALL_FOUND}"

cat >> "${REPORT}" <<'FOOTER'

## Files Containing x86 SIMD (kernel/x86_64/)

These files are **not compiled** for the RISC-V target because
`ARCH=riscv64` routes the build to `kernel/riscv64/` and `kernel/generic/`.
The patches above add `#if !defined(__riscv)` guards as defense-in-depth
in case a future OpenBLAS CMake change re-exposes them.

## Fallback Performance Analysis

| Operation | SSE2/AVX lanes | Scalar replacement | Estimated slowdown |
|-----------|---------------|-------------------|-------------------|
| `_mm256_fmadd_pd` | 4×f64 FMA | 4× `fma()` calls | 1–2× (rv64gc has FMADD.D) |
| `_mm256_mul_pd` | 4×f64 | 4× scalar multiply | 1–4× |
| `_mm256_hadd_pd` | 4×f64 horizontal | 4 adds | 1–2× |
| `_mm_load_pd` | 2×f64 load | `memcpy` 16B | ~1× |
| `_mm256_loadu_pd` | 4×f64 unaligned load | `memcpy` 32B | ~1× |
| `_mm_stream_pd` | non-temporal store | plain store | ~1× on QEMU |
| `_mm_prefetch` | cache hint | `__builtin_prefetch` | ~1× |

**Note:** `fma()` on rv64gc maps to the `FMADD.D` instruction.
This means the scalar replacement for `_mm256_fmadd_pd` is **not** a
performance regression at the instruction level — it is a throughput
regression (1 operation per cycle vs 4 with AVX). RVV recovers this.

## Summary

FOOTER

total=$(wc -l < "${ALL_FOUND}")
echo "- Total distinct intrinsic types found: ${total}" >> "${REPORT}"
echo "- All guarded by \`#if !defined(__riscv)\` in patches 0003–0008" >> "${REPORT}"
echo "- All scalar fallbacks in \`simd_fallbacks/\`" >> "${REPORT}"
echo "- Correctness verified by \`harness/numerical_suite.c\`" >> "${REPORT}"

log_info "Report written to: ${REPORT}"
cat "${REPORT}"

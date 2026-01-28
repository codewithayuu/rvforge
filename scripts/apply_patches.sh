#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PATCHES_DIR="${ROOT}/patches"
EXTERN_DIR="${ROOT}/extern"
BUILD_DIR="${ROOT}/build"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[PATCH]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}   $*"; }
log_error() { echo -e "${RED}[ERROR]${NC}  $*"; }
log_step()  { echo -e "${CYAN}[STEP]${NC}   $*"; }

GMP_VERSION=6.3.0
MPFR_VERSION=4.2.1
OPENBLAS_VERSION=0.3.26

GMP_SRC="${EXTERN_DIR}/gmp-${GMP_VERSION}"
MPFR_SRC="${EXTERN_DIR}/mpfr-${MPFR_VERSION}"
OPENBLAS_SRC="${EXTERN_DIR}/openblas-${OPENBLAS_VERSION}"

PATCH_LOG="${ROOT}/docs/errors/patch_apply.log"
mkdir -p "${ROOT}/docs/errors"
: > "${PATCH_LOG}"

log_patch() {
    local patch_file="$1"
    local target_dir="$2"
    local patch_name
    patch_name=$(basename "${patch_file}")

    log_step "Applying ${patch_name} to $(basename ${target_dir})"

    if [ ! -d "${target_dir}" ]; then
        log_warn "Source dir not found: ${target_dir} (will be fetched by CMake)"
        return 0
    fi

    if patch --dry-run -p1 -d "${target_dir}" < "${patch_file}" &>/dev/null; then
        patch -p1 -d "${target_dir}" < "${patch_file}" \
            | tee -a "${PATCH_LOG}"
        log_info "${patch_name}: applied cleanly"
    else
        if patch -R --dry-run -p1 -d "${target_dir}" < "${patch_file}" &>/dev/null; then
            log_warn "${patch_name}: already applied, skipping"
        else
            log_error "${patch_name}: failed to apply"
            patch --dry-run -p1 -d "${target_dir}" < "${patch_file}" 2>&1 \
                | tee -a "${PATCH_LOG}"
            return 1
        fi
    fi
}

verify_patch_integrity() {
    log_step "Verifying patch file integrity..."
    local all_ok=0
    for patch_file in "${PATCHES_DIR}"/*.patch; do
        if [ ! -f "${patch_file}" ]; then
            log_warn "No .patch files found in ${PATCHES_DIR}"
            break
        fi
        if grep -q "^diff --git" "${patch_file}" || \
           grep -q "^---" "${patch_file}"; then
            log_info "$(basename ${patch_file}): valid patch format"
        else
            log_warn "$(basename ${patch_file}): may not be valid unified diff"
            all_ok=1
        fi
    done
    return $all_ok
}

apply_gmp_patches() {
    log_step "=== GMP Patches ==="
    log_patch \
        "${PATCHES_DIR}/0001-gmp-riscv-host-detect.patch" \
        "${GMP_SRC}"
}

apply_openblas_patches() {
    log_step "=== OpenBLAS Patches ==="
    log_patch \
        "${PATCHES_DIR}/0002-openblas-riscv64-target.patch" \
        "${OPENBLAS_SRC}"
    log_patch \
        "${PATCHES_DIR}/0003-openblas-cpuid-guard.patch" \
        "${OPENBLAS_SRC}"
}

post_patch_verify() {
    log_step "Post-patch verification..."

    if [ -d "${OPENBLAS_SRC}" ]; then
        local cpuid_file="${OPENBLAS_SRC}/driver/others/cpuid_x86.c"
        if [ -f "${cpuid_file}" ]; then
            if grep -q "__riscv" "${cpuid_file}"; then
                log_info "cpuid_x86.c: riscv guard present"
            else
                log_warn "cpuid_x86.c: riscv guard missing"
            fi
        fi

        local cmake_file="${OPENBLAS_SRC}/CMakeLists.txt"
        if [ -f "${cmake_file}" ]; then
            if grep -q "RISCV64_GENERIC" "${cmake_file}"; then
                log_info "CMakeLists.txt: RISCV64_GENERIC target present"
            else
                log_warn "CMakeLists.txt: RISCV64_GENERIC not found"
            fi
        fi
    fi

    if [ -d "${GMP_SRC}" ]; then
        local gmp_conf="${GMP_SRC}/configure.ac"
        if [ -f "${gmp_conf}" ]; then
            if grep -q "riscv64" "${gmp_conf}"; then
                log_info "GMP configure.ac: riscv64 case present"
            else
                log_warn "GMP configure.ac: riscv64 case missing"
            fi
        fi
    fi
}

main() {
    log_info "=== RVForge Patch Application ==="
    log_info "Patches dir: ${PATCHES_DIR}"
    log_info "Log: ${PATCH_LOG}"

    verify_patch_integrity

    apply_gmp_patches
    apply_openblas_patches
    post_patch_verify

    log_info "=== Patch application complete ==="
    log_info "See ${PATCH_LOG} for full output"
}

main "$@"

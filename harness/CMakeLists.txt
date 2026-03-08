include(${RVFORGE_ROOT}/toolchain/flags.cmake)
include(${RVFORGE_ROOT}/cmake/RVForgeUtils.cmake)

find_program(QEMU_BIN qemu-riscv64-static REQUIRED)

set(HARNESS_INCLUDE_DIRS
    ${RVFORGE_INSTALL_DIR}/mpfr/include
    ${RVFORGE_INSTALL_DIR}/gmp/include
    ${RVFORGE_INSTALL_DIR}/openblas/include
    ${RVFORGE_ROOT}/simd_fallbacks
)

set(MPFR_STATIC_LIBS
    ${RVFORGE_INSTALL_DIR}/mpfr/lib/libmpfr.a
    ${RVFORGE_INSTALL_DIR}/gmp/lib/libgmp.a
)

set(BLAS_STATIC_LIBS
    ${RVFORGE_INSTALL_DIR}/openblas/lib/libopenblas.a
)

add_executable(mpfr_smoke mpfr_smoke.c)
target_include_directories(mpfr_smoke PRIVATE ${HARNESS_INCLUDE_DIRS})
target_link_libraries(mpfr_smoke PRIVATE ${MPFR_STATIC_LIBS} m)
rvforge_apply_flags(mpfr_smoke)
rvforge_record_size(mpfr_smoke)
add_dependencies(mpfr_smoke mpfr_riscv gmp_riscv)

add_executable(blas_smoke blas_smoke.c)
target_include_directories(blas_smoke PRIVATE ${HARNESS_INCLUDE_DIRS})
target_link_libraries(blas_smoke PRIVATE ${BLAS_STATIC_LIBS} m)
rvforge_apply_flags(blas_smoke)
rvforge_record_size(blas_smoke)
add_dependencies(blas_smoke openblas_riscv)

add_executable(numerical_suite numerical_suite.c)
target_include_directories(numerical_suite PRIVATE ${HARNESS_INCLUDE_DIRS})
target_link_libraries(numerical_suite PRIVATE ${BLAS_STATIC_LIBS} m)
rvforge_apply_flags(numerical_suite)
rvforge_record_size(numerical_suite)
add_dependencies(numerical_suite openblas_riscv)

add_executable(simd_audit_report
    simd_audit_report.c
    ${RVFORGE_ROOT}/simd_fallbacks/scalar_dgemv.c
    ${RVFORGE_ROOT}/simd_fallbacks/scalar_dgemm.c
    ${RVFORGE_ROOT}/simd_fallbacks/scalar_sgemm.c
    ${RVFORGE_ROOT}/simd_fallbacks/scalar_memory.c
)
target_include_directories(simd_audit_report PRIVATE ${HARNESS_INCLUDE_DIRS})
target_link_libraries(simd_audit_report PRIVATE m)
rvforge_apply_flags(simd_audit_report)
rvforge_record_size(simd_audit_report)

set_target_properties(
    mpfr_smoke blas_smoke numerical_suite simd_audit_report
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${RVFORGE_BUILD_DIR}/bin
)

foreach(tgt IN ITEMS mpfr_smoke blas_smoke numerical_suite simd_audit_report)
    add_custom_target(run_${tgt}
        COMMAND ${QEMU_BIN} -L /usr/riscv64-linux-gnu
                ${RVFORGE_BUILD_DIR}/bin/${tgt}
        DEPENDS ${tgt}
        COMMENT "QEMU: ${tgt}"
        VERBATIM
    )
endforeach()

add_custom_target(run_all_
    DEPENDS
        run_mpfr_smoke
        run_blas_smoke
        run_numerical_suite
        run_simd_audit_report
    COMMENT "all tests complete"
)

if(RVFORGE_RUN_TESTS)
    rvforge_add_qemu_test(mpfr_smoke)
    rvforge_add_qemu_test(blas_smoke)
    rvforge_add_qemu_test(numerical_suite)
    rvforge_add_qemu_test(simd_audit_report)
endif()

add_custom_target(audit_binaries
    COMMAND sh -c "echo '--- mpfr_smoke ELF ---' && \
        riscv64-linux-gnu-readelf -h ${RVFORGE_BUILD_DIR}/bin/mpfr_smoke | grep -E 'Class|Machine|Type' && \
        echo '--- blas_smoke ELF ---' && \
        riscv64-linux-gnu-readelf -h ${RVFORGE_BUILD_DIR}/bin/blas_smoke | grep -E 'Class|Machine|Type'"
    DEPENDS mpfr_smoke blas_smoke
    COMMENT "Auditing ELF headers"
    VERBATIM
)

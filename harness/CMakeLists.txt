include(${RVFORGE_ROOT}/toolchain/flags.cmake)
include(${RVFORGE_ROOT}/cmake/RVForgeUtils.cmake)

find_program(QEMU_BIN qemu-riscv64-static REQUIRED)

set(HARNESS_INCLUDE_DIRS
    ${RVFORGE_INSTALL_DIR}/mpfr/include
    ${RVFORGE_INSTALL_DIR}/gmp/include
    ${RVFORGE_INSTALL_DIR}/openblas/include
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

set_target_properties(mpfr_smoke blas_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${RVFORGE_BUILD_DIR}/bin
)

add_custom_target(run_mpfr_smoke
    COMMAND ${QEMU_BIN} -L /usr/riscv64-linux-gnu
            ${RVFORGE_BUILD_DIR}/bin/mpfr_smoke
    DEPENDS mpfr_smoke
    COMMENT "Running mpfr_smoke under qemu-riscv64-static"
    VERBATIM
)

add_custom_target(run_blas_smoke
    COMMAND ${QEMU_BIN} -L /usr/riscv64-linux-gnu
            ${RVFORGE_BUILD_DIR}/bin/blas_smoke
    DEPENDS blas_smoke
    COMMENT "Running blas_smoke under qemu-riscv64-static"
    VERBATIM
)

add_custom_target(run_all_smoke
    DEPENDS run_mpfr_smoke run_blas_smoke
    COMMENT "All smoke tests complete"
)

if(RVFORGE_RUN_TESTS)
    rvforge_add_qemu_test(mpfr_smoke)
    rvforge_add_qemu_test(blas_smoke)
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

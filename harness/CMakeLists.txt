include(${RVFORGE_ROOT}/toolchain/flags.cmake)
include(${RVFORGE_ROOT}/cmake/RVForgeUtils.cmake)

find_program(QEMU_BIN qemu-riscv64-static REQUIRED)

set(HARNESS_INCLUDES
    ${RVFORGE_INSTALL_DIR}/mpfr/include
    ${RVFORGE_INSTALL_DIR}/gmp/include
    ${RVFORGE_INSTALL_DIR}/openblas/include
    ${RVFORGE_ROOT}/simd_fallbacks
    ${RVFORGE_ROOT}/validation
    ${RVFORGE_ROOT}/rvv
)

set(RVV_SRCS
    ${RVFORGE_ROOT}/rvv/rvv_ddot.c
    ${RVFORGE_ROOT}/rvv/rvv_daxpy.c
    ${RVFORGE_ROOT}/rvv/rvv_dnrm2.c
    ${RVFORGE_ROOT}/rvv/rvv_dgemv.c
    ${RVFORGE_ROOT}/rvv/rvv_dgemm.c
)

if(RVFORGE_ENABLE_RVV)
    set(RVV_COMPILE_FLAGS -march=rv64gcv -mabi=lp64d -O2
                          -fno-stack-protector -ffunction-sections)
else()
    set(RVV_COMPILE_FLAGS -march=rv64gc -mabi=lp64d -O2
                          -fno-stack-protector -ffunction-sections)
endif()

set(MPFR_LIBS
    ${RVFORGE_INSTALL_DIR}/mpfr/lib/libmpfr.a
    ${RVFORGE_INSTALL_DIR}/gmp/lib/libgmp.a
)

set(BLAS_LIBS
    ${RVFORGE_INSTALL_DIR}/openblas/lib/libopenblas.a
)

set(SIMD_FALLBACK_SRCS
    ${RVFORGE_ROOT}/simd_fallbacks/scalar_dgemv.c
    ${RVFORGE_ROOT}/simd_fallbacks/scalar_dgemm.c
    ${RVFORGE_ROOT}/simd_fallbacks/scalar_sgemm.c
    ${RVFORGE_ROOT}/simd_fallbacks/scalar_memory.c
)

function(add_harness_target name)
    cmake_parse_arguments(A "" "" "SRCS;LIBS;DEPS;EXTRA_FLAGS" ${ARGN})
    add_executable(${name} ${A_SRCS})
    target_include_directories(${name} PRIVATE ${HARNESS_INCLUDES})
    target_link_libraries(${name} PRIVATE ${A_LIBS} m)
    rvforge_apply_flags(${name})
    if(A_EXTRA_FLAGS)
        target_compile_options(${name} PRIVATE ${A_EXTRA_FLAGS})
    endif()
    rvforge_record_size(${name})
    if(A_DEPS)
        add_dependencies(${name} ${A_DEPS})
    endif()
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${RVFORGE_BUILD_DIR}/bin
    )
    add_custom_target(run_${name}
        COMMAND ${QEMU_BIN} -L /usr/riscv64-linux-gnu
                ${RVFORGE_BUILD_DIR}/bin/${name}
        DEPENDS ${name}
        COMMENT "QEMU: ${name}"
        VERBATIM
    )
    if(RVFORGE_RUN_TESTS)
        rvforge_add_qemu_test(${name})
    endif()
endfunction()

if(RVFORGE_BUILD_MPFR)
    add_harness_target(mpfr_smoke
        SRCS mpfr_smoke.c
        LIBS ${MPFR_LIBS}
        DEPS mpfr_riscv gmp_riscv
    )

    add_harness_target(mpfr_full_suite
        SRCS mpfr_full_suite.c
        LIBS ${MPFR_LIBS}
        DEPS mpfr_riscv gmp_riscv
    )
endif()

if(RVFORGE_BUILD_OPENBLAS)
    add_harness_target(blas_smoke
        SRCS blas_smoke.c
        LIBS ${BLAS_LIBS}
        DEPS openblas_riscv
    )

    add_harness_target(numerical_suite
        SRCS numerical_suite.c
        LIBS ${BLAS_LIBS}
        DEPS openblas_riscv
    )

    add_harness_target(blas_full_suite
        SRCS blas_full_suite.c
        LIBS ${BLAS_LIBS}
        DEPS openblas_riscv
    )
endif()

add_harness_target(simd_audit_report
    SRCS simd_audit_report.c ${SIMD_FALLBACK_SRCS}
    LIBS ""
    DEPS ""
)

add_harness_target(blas_full_suite
    SRCS blas_full_suite.c
    LIBS ${BLAS_LIBS}
    DEPS openblas_riscv
)

add_harness_target(rvv_correctness
    SRCS rvv_correctness.c ${RVV_SRCS}
    LIBS ""
    EXTRA_FLAGS ${RVV_COMPILE_FLAGS})

add_harness_target(rvv_vs_scalar_bench
    SRCS rvv_vs_scalar_bench.c ${RVV_SRCS}
    LIBS ""
    EXTRA_FLAGS ${RVV_COMPILE_FLAGS})

set(VALIDATE_DEPS run_simd_audit_report)
set(RUN_ALL_DEPS ${VALIDATE_DEPS} run_rvv_correctness run_rvv_vs_scalar_bench)

if(RVFORGE_BUILD_MPFR)
    list(APPEND VALIDATE_DEPS run_mpfr_smoke run_mpfr_full_suite)
endif()

if(RVFORGE_BUILD_OPENBLAS)
    list(APPEND VALIDATE_DEPS run_blas_smoke run_numerical_suite run_blas_full_suite)
endif()

list(APPEND RUN_ALL_DEPS ${VALIDATE_DEPS})
list(REMOVE_DUPLICATES RUN_ALL_DEPS)

add_custom_target(run_all_
    DEPENDS ${RUN_ALL_DEPS}
    COMMENT "all targets complete"
)

add_custom_target(validate
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/validate_qemu.sh
    DEPENDS ${VALIDATE_DEPS}
    COMMENT "Running full QEMU validation pipeline"
    VERBATIM
)

add_custom_target(bench_rvv
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/bench_rvv.sh
    COMMENT "Running RVV benchmark suite"
    VERBATIM
)

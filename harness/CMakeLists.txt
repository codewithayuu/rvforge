set(HARNESS_SRC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(mpfr_smoke ${HARNESS_SRC}/mpfr_smoke.c)

target_include_directories(mpfr_smoke PRIVATE
    ${RVFORGE_INSTALL_DIR}/mpfr/include
    ${RVFORGE_INSTALL_DIR}/gmp/include
)

target_link_libraries(mpfr_smoke PRIVATE
    ${RVFORGE_INSTALL_DIR}/mpfr/lib/libmpfr.a
    ${RVFORGE_INSTALL_DIR}/gmp/lib/libgmp.a
    m
)

target_compile_options(mpfr_smoke PRIVATE -march=rv64gc -mabi=lp64d)

add_dependencies(mpfr_smoke mpfr_riscv gmp_riscv)

add_executable(blas_smoke ${HARNESS_SRC}/blas_smoke.c)

target_include_directories(blas_smoke PRIVATE
    ${RVFORGE_INSTALL_DIR}/openblas/include
)

target_link_libraries(blas_smoke PRIVATE
    ${RVFORGE_INSTALL_DIR}/openblas/lib/libopenblas.a
    m
)

target_compile_options(blas_smoke PRIVATE -march=rv64gc -mabi=lp64d)

add_dependencies(blas_smoke openblas_riscv)

set_target_properties(mpfr_smoke blas_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${RVFORGE_BUILD_DIR}/bin
)

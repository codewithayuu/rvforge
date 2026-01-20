set(OPENBLAS_VERSION  0.3.26)
set(OPENBLAS_URL      https://github.com/OpenMathLib/OpenBLAS/releases/download/v${OPENBLAS_VERSION}/OpenBLAS-${OPENBLAS_VERSION}.tar.gz)
set(OPENBLAS_SHA256   4b326a2748d914e5b40bb65846df6e9ca2d6c2b3825047e5f5b37e5be15b8ca2)
set(OPENBLAS_SRC_DIR  ${RVFORGE_EXTERN}/openblas)
set(OPENBLAS_BUILD_DIR ${RVFORGE_BUILD_DIR}/openblas-build)
set(OPENBLAS_INSTALL  ${RVFORGE_INSTALL_DIR}/openblas)

ExternalProject_Add(openblas_riscv
    URL            ${OPENBLAS_URL}
    URL_HASH       SHA256=${OPENBLAS_SHA256}
    SOURCE_DIR     ${OPENBLAS_SRC_DIR}
    BINARY_DIR     ${OPENBLAS_BUILD_DIR}
    INSTALL_DIR    ${OPENBLAS_INSTALL}
    PATCH_COMMAND
        patch -p1 -d ${OPENBLAS_SRC_DIR}
            < ${RVFORGE_PATCHES}/0002-openblas-disable-x86-kernel.patch || true
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${RVFORGE_ROOT}/toolchain/riscv64-linux-gnu.cmake
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DTARGET=RISCV64_GENERIC
        -DARCH=riscv64
        -DCORE=RISCV64_GENERIC
        -DBINARY=64
        -DINTERFACE64=0
        -DUSE_THREAD=0
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTING=ON
        -DNO_AVX=1
        -DNO_AVX2=1
        -DNO_SSE=1
        -DNO_SSE2=1
        -DNOFORTRAN=1
        -DCMAKE_C_FLAGS=${COMMON_CFLAGS}
        -DCMAKE_BUILD_TYPE=Release
    BUILD_COMMAND   cmake --build <BINARY_DIR> -- -j${NPROC}
    INSTALL_COMMAND cmake --install <BINARY_DIR>
    LOG_CONFIGURE ON
    LOG_BUILD     ON
    LOG_INSTALL   ON
)

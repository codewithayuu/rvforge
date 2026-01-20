set(MPFR_VERSION   4.2.1)
set(MPFR_TARBALL   mpfr-${MPFR_VERSION}.tar.xz)
set(MPFR_URL       https://www.mpfr.org/mpfr-current/${MPFR_TARBALL})
set(MPFR_SHA256    277807353a6726978996945af13e52829e3abd7a9a5b7fb2793894e18f1fcbb2)
set(MPFR_SRC_DIR   ${RVFORGE_EXTERN}/mpfr)
set(MPFR_BUILD_DIR ${RVFORGE_BUILD_DIR}/mpfr-build)
set(MPFR_INSTALL   ${RVFORGE_INSTALL_DIR}/mpfr)

set(GMP_VERSION    6.3.0)
set(GMP_TARBALL    gmp-${GMP_VERSION}.tar.xz)
set(GMP_URL        https://gmplib.org/download/gmp/${GMP_TARBALL})
set(GMP_SHA256     a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898)
set(GMP_SRC_DIR    ${RVFORGE_EXTERN}/gmp)
set(GMP_BUILD_DIR  ${RVFORGE_BUILD_DIR}/gmp-build)
set(GMP_INSTALL    ${RVFORGE_INSTALL_DIR}/gmp)

ExternalProject_Add(gmp_riscv
    URL            ${GMP_URL}
    URL_HASH       SHA256=${GMP_SHA256}
    SOURCE_DIR     ${GMP_SRC_DIR}
    BINARY_DIR     ${GMP_BUILD_DIR}
    INSTALL_DIR    ${GMP_INSTALL}
    CONFIGURE_COMMAND
        ${GMP_SRC_DIR}/configure
            --host=${RISCV_TRIPLE}
            --build=x86_64-linux-gnu
            --prefix=<INSTALL_DIR>
            --enable-static
            --disable-shared
            CC=${CROSS_C_COMPILER}
            CXX=${CROSS_CXX_COMPILER}
            AR=${CROSS_AR}
            RANLIB=${CROSS_RANLIB}
            CFLAGS=${COMMON_CFLAGS}
    BUILD_COMMAND   make -j${NPROC}
    INSTALL_COMMAND make install
    LOG_CONFIGURE ON
    LOG_BUILD     ON
    LOG_INSTALL   ON
)

ExternalProject_Add(mpfr_riscv
    DEPENDS        gmp_riscv
    URL            ${MPFR_URL}
    URL_HASH       SHA256=${MPFR_SHA256}
    SOURCE_DIR     ${MPFR_SRC_DIR}
    BINARY_DIR     ${MPFR_BUILD_DIR}
    INSTALL_DIR    ${MPFR_INSTALL}
    CONFIGURE_COMMAND
        ${MPFR_SRC_DIR}/configure
            --host=${RISCV_TRIPLE}
            --build=x86_64-linux-gnu
            --prefix=<INSTALL_DIR>
            --enable-static
            --disable-shared
            --with-gmp=${GMP_INSTALL}
            CC=${CROSS_C_COMPILER}
            AR=${CROSS_AR}
            RANLIB=${CROSS_RANLIB}
            CFLAGS=${COMMON_CFLAGS}
    BUILD_COMMAND   make -j${NPROC}
    INSTALL_COMMAND make install
    LOG_CONFIGURE ON
    LOG_BUILD     ON
    LOG_INSTALL   ON
)

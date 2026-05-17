set(GMP_VERSION   6.3.0)
set(GMP_TARBALL   gmp-${GMP_VERSION}.tar.xz)
set(GMP_URL       https://ftp.gnu.org/gnu/gmp/${GMP_TARBALL})
set(GMP_SHA256    a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898)
set(GMP_SRC_DIR   ${RVFORGE_EXTERN}/gmp-${GMP_VERSION})
set(GMP_BUILD_DIR ${RVFORGE_BUILD_DIR}/gmp-build)
set(GMP_INSTALL   ${RVFORGE_INSTALL_DIR}/gmp)

set(MPFR_VERSION   4.2.1)
set(MPFR_TARBALL   mpfr-${MPFR_VERSION}.tar.xz)
set(MPFR_URL       https://ftp.gnu.org/gnu/mpfr/${MPFR_TARBALL})
set(MPFR_SHA256    277807353a6726978996945af13e52829e3abd7a9a5b7fb2793894e18f1fcbb2)
set(MPFR_SRC_DIR   ${RVFORGE_EXTERN}/mpfr-${MPFR_VERSION})
set(MPFR_BUILD_DIR ${RVFORGE_BUILD_DIR}/mpfr-build)
set(MPFR_INSTALL   ${RVFORGE_INSTALL_DIR}/mpfr)

set(CROSS_ENV
    CC=${RISCV_TRIPLE}-gcc
    CXX=${RISCV_TRIPLE}-g++
    AR=${RISCV_TRIPLE}-ar
    RANLIB=${RISCV_TRIPLE}-ranlib
    NM=${RISCV_TRIPLE}-nm
    STRIP=${RISCV_TRIPLE}-strip
    CFLAGS=${COMMON_CFLAGS}
    CXXFLAGS=${COMMON_CFLAGS}
    PKG_CONFIG_PATH=
    PKG_CONFIG_LIBDIR=
    PKG_CONFIG=/bin/false
)

ExternalProject_Add(gmp_riscv
    URL               ${GMP_URL}
    URL_HASH          SHA256=${GMP_SHA256}
    SOURCE_DIR        ${GMP_SRC_DIR}
    BINARY_DIR        ${GMP_BUILD_DIR}
    INSTALL_DIR       ${GMP_INSTALL}
    DOWNLOAD_EXTRACT_TIMESTAMP true
    PATCH_COMMAND
        cd <SOURCE_DIR> &&
        patch -p1 < ${RVFORGE_PATCHES}/0001-gmp-riscv-host-detect.patch || true
    CONFIGURE_COMMAND
        ${GMP_SRC_DIR}/configure
            --host=${RISCV_TRIPLE}
            --build=x86_64-linux-gnu
            --prefix=<INSTALL_DIR>
            --enable-static
            --disable-shared
            --disable-assembly
            --with-pic
            ${CROSS_ENV}
    BUILD_COMMAND
        $(MAKE) -j${NPROC} MAKEFLAGS=
    INSTALL_COMMAND
        $(MAKE) install
    BUILD_IN_SOURCE   0
    LOG_CONFIGURE     ON
    LOG_BUILD         ON
    LOG_INSTALL       ON
    LOG_MERGED_STDOUTERR ON
)

ExternalProject_Add_Step(gmp_riscv verify_elf
    COMMAND
        riscv64-linux-gnu-readelf -h ${GMP_INSTALL}/lib/libgmp.a
        | grep -E "Class|Machine" | head -4
    DEPENDEES install
    LOG ON
)

ExternalProject_Add(mpfr_riscv
    DEPENDS           gmp_riscv
    URL               ${MPFR_URL}
    URL_HASH          SHA256=${MPFR_SHA256}
    SOURCE_DIR        ${MPFR_SRC_DIR}
    BINARY_DIR        ${MPFR_BUILD_DIR}
    INSTALL_DIR       ${MPFR_INSTALL}
    DOWNLOAD_EXTRACT_TIMESTAMP true
    CONFIGURE_COMMAND
        ${MPFR_SRC_DIR}/configure
            --host=${RISCV_TRIPLE}
            --build=x86_64-linux-gnu
            --prefix=<INSTALL_DIR>
            --enable-static
            --disable-shared
            --with-pic
            --with-gmp=${GMP_INSTALL}
            --with-gmp-include=${GMP_INSTALL}/include
            --with-gmp-lib=${GMP_INSTALL}/lib
            --disable-assert
            ${CROSS_ENV}
    BUILD_COMMAND
        $(MAKE) -j${NPROC} MAKEFLAGS=
    INSTALL_COMMAND
        $(MAKE) install
    BUILD_IN_SOURCE   0
    LOG_CONFIGURE     ON
    LOG_BUILD         ON
    LOG_INSTALL       ON
    LOG_MERGED_STDOUTERR ON
)

ExternalProject_Add_Step(mpfr_riscv verify_elf
    COMMAND
        riscv64-linux-gnu-readelf -h ${MPFR_INSTALL}/lib/libmpfr.a
        | grep -E "Class|Machine" | head -4
    DEPENDEES install
    LOG ON
)

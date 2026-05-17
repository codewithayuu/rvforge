set(OPENBLAS_VERSION   0.3.26)
set(OPENBLAS_TARBALL   OpenBLAS-${OPENBLAS_VERSION}.tar.gz)
set(OPENBLAS_URL       https://github.com/OpenMathLib/OpenBLAS/releases/download/v${OPENBLAS_VERSION}/${OPENBLAS_TARBALL})
set(OPENBLAS_SHA256    4e6e4f5cb14c209262e33e6816d70221a2fe49eb69eaf0a06f065598ac602c68)
set(OPENBLAS_SRC_DIR   ${RVFORGE_EXTERN}/openblas-${OPENBLAS_VERSION})
set(OPENBLAS_BUILD_DIR ${RVFORGE_BUILD_DIR}/openblas-build)
set(OPENBLAS_INSTALL   ${RVFORGE_INSTALL_DIR}/openblas)

set(OPENBLAS_PATCHES
    ${RVFORGE_PATCHES}/0002-openblas-riscv64-target.patch
    ${RVFORGE_PATCHES}/0003-openblas-cpuid-guard.patch
)

set(OPENBLAS_PATCH_CMD
    cd <SOURCE_DIR>
)
foreach(p ${OPENBLAS_PATCHES})
    list(APPEND OPENBLAS_PATCH_CMD && patch -p1 < ${p})
endforeach()
list(APPEND OPENBLAS_PATCH_CMD "&&" "sed" "-i" "s/add_subdirectory(utest)/#add_subdirectory(utest)/g" "CMakeLists.txt")
list(APPEND OPENBLAS_PATCH_CMD "&&" "sed" "-i" "s/add_subdirectory(ctest)/#add_subdirectory(ctest)/g" "CMakeLists.txt")

ExternalProject_Add(openblas_riscv
    URL               ${OPENBLAS_URL}
    URL_HASH          SHA256=${OPENBLAS_SHA256}
    SOURCE_DIR        ${OPENBLAS_SRC_DIR}
    BINARY_DIR        ${OPENBLAS_BUILD_DIR}
    INSTALL_DIR       ${OPENBLAS_INSTALL}
    DOWNLOAD_EXTRACT_TIMESTAMP true

    PATCH_COMMAND     ${OPENBLAS_PATCH_CMD}

    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${RVFORGE_ROOT}/toolchain/riscv64-linux-gnu.cmake
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        "-DCMAKE_C_FLAGS=${COMMON_CFLAGS} -std=gnu89 -DDTB_DEFAULT_ENTRIES=64"
        -DTARGET=RISCV64_GENERIC
        -DARCH=riscv64
        -DCORE=RISCV64_GENERIC
        -DBINARY=64
        -DINTERFACE64=0
        -DUSE_THREAD=0
        -DUSE_OPENMP=0
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTING=OFF
        -DNO_AVX=1
        -DNO_AVX2=1
        -DNO_AVX512=1
        -DNO_SSE=1
        -DNO_SSE2=1
        -DNO_SSE3=1
        -DNO_SSSE3=1
        -DNO_SSE4_1=1
        -DNO_SSE4_2=1
        -DNOFORTRAN=1
        -DNO_LAPACK=0
        -DDYNAMIC_ARCH=OFF
        -DDYNAMIC_OLDER=OFF
        -DNO_WARMUP=1
        -DMAX_STACK_ALLOC=2048

    BUILD_COMMAND
        cmake --build <BINARY_DIR> -- -j${NPROC}

    INSTALL_COMMAND
        cmake --install <BINARY_DIR>

    LOG_CONFIGURE     ON
    LOG_BUILD         ON
    LOG_INSTALL       ON
    LOG_MERGED_STDOUTERR ON
)

ExternalProject_Add_Step(openblas_riscv verify_elf
    COMMAND sh -c "riscv64-linux-gnu-readelf -h ${OPENBLAS_INSTALL}/lib/libopenblas.a | grep -E 'Class|Machine' | head -4"
    DEPENDEES install
    LOG ON
)

ExternalProject_Add_Step(openblas_riscv audit_x86_symbols
    COMMAND
        sh -c "riscv64-linux-gnu-objdump -d ${OPENBLAS_INSTALL}/lib/libopenblas.a 2>/dev/null \
            | grep -c 'xmm\\|ymm\\|_mm_\\|__SSE' && echo 'FAIL: x86 symbols found' && exit 1 \
            || echo 'PASS: no x86 symbols'"
    DEPENDEES verify_elf
    LOG ON
)

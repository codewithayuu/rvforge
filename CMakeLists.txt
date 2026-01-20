cmake_minimum_required(VERSION 3.22)
project(RVForge LANGUAGES C CXX VERSION 1.0.0)

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RVFORGE_BUILD_MPFR     "Build MPFR port"     ON)
option(RVFORGE_BUILD_OPENBLAS "Build OpenBLAS port"  ON)
option(RVFORGE_ENABLE_RVV     "Enable RVV extensions" OFF)
option(RVFORGE_STATIC         "Force static linking"  ON)

set(RVFORGE_ROOT        ${CMAKE_SOURCE_DIR})
set(RVFORGE_EXTERN      ${RVFORGE_ROOT}/extern)
set(RVFORGE_PATCHES     ${RVFORGE_ROOT}/patches)
set(RVFORGE_BUILD_DIR   ${CMAKE_BINARY_DIR})
set(RVFORGE_INSTALL_DIR ${CMAKE_BINARY_DIR}/install)

include(ExternalProject)
include(ProcessorCount)

ProcessorCount(NPROC)
if(NPROC EQUAL 0)
    set(NPROC 4)
endif()

set(RISCV_TRIPLE riscv64-linux-gnu)
set(CROSS_C_COMPILER   ${RISCV_TRIPLE}-gcc)
set(CROSS_CXX_COMPILER ${RISCV_TRIPLE}-g++)
set(CROSS_AR           ${RISCV_TRIPLE}-ar)
set(CROSS_RANLIB       ${RISCV_TRIPLE}-ranlib)

set(COMMON_CFLAGS "-march=rv64gc -mabi=lp64d -O2 -fPIC")

if(RVFORGE_BUILD_MPFR)
    add_subdirectory(ports/mpfr)
endif()

if(RVFORGE_BUILD_OPENBLAS)
    add_subdirectory(ports/openblas)
endif()

add_subdirectory(harness)

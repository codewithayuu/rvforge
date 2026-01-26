cmake_minimum_required(VERSION 3.22)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

project(RVForge LANGUAGES C CXX VERSION 1.0.0)

set(CMAKE_C_STANDARD            11)
set(CMAKE_C_STANDARD_REQUIRED   ON)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(RVForgeUtils)
include(ExternalProject)
include(ProcessorCount)
include(CTest)

rvforge_assert_riscv()

option(RVFORGE_BUILD_MPFR     "Build MPFR port"      ON)
option(RVFORGE_BUILD_OPENBLAS "Build OpenBLAS port"   ON)
option(RVFORGE_ENABLE_RVV     "Enable RVV extensions" OFF)
option(RVFORGE_STATIC         "Force static linking"  ON)
option(RVFORGE_RUN_TESTS      "Enable CTest targets"  ON)

set(RVFORGE_ROOT        ${CMAKE_SOURCE_DIR}           CACHE PATH "" FORCE)
set(RVFORGE_EXTERN      ${RVFORGE_ROOT}/extern)
set(RVFORGE_PATCHES     ${RVFORGE_ROOT}/patches)
set(RVFORGE_BUILD_DIR   ${CMAKE_BINARY_DIR})
set(RVFORGE_INSTALL_DIR ${CMAKE_BINARY_DIR}/install)

ProcessorCount(NPROC)
if(NPROC EQUAL 0)
    set(NPROC 4)
endif()

set(RISCV_TRIPLE    riscv64-linux-gnu)
set(RISCV_MARCH     rv64gc)
set(RISCV_MABI      lp64d)
set(COMMON_CFLAGS   "-march=${RISCV_MARCH} -mabi=${RISCV_MABI} -O2 -fno-stack-protector -ffunction-sections -fdata-sections")

include(${RVFORGE_ROOT}/toolchain/flags.cmake)

if(RVFORGE_BUILD_MPFR)
    add_subdirectory(ports/mpfr)
endif()

if(RVFORGE_BUILD_OPENBLAS)
    add_subdirectory(ports/openblas)
endif()

add_subdirectory(harness)

if(RVFORGE_RUN_TESTS)
    enable_testing()
endif()

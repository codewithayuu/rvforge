if(NOT DEFINED RISCV_MARCH)
    set(RISCV_MARCH "rv64gc")
endif()

if(NOT DEFINED RISCV_MABI)
    set(RISCV_MABI "lp64d")
endif()

set(RVFORGE_C_FLAGS
    -march=${RISCV_MARCH}
    -mabi=${RISCV_MABI}
    -O2
    -fno-stack-protector
    -fno-exceptions
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wno-unused-parameter
)

set(RVFORGE_LINK_FLAGS
    -static
    -static-libgcc
    -Wl,--gc-sections
)

set(RVFORGE_DEFINES
    -D_GNU_SOURCE
    -D_REENTRANT
)

if(RVFORGE_ENABLE_RVV)
    string(REPLACE "rv64gc" "rv64gcv" RVFORGE_RVV_MARCH "${RISCV_MARCH}")
    set(RVFORGE_RVV_FLAGS
        -march=${RVFORGE_RVV_MARCH}
        -mabi=${RISCV_MABI}
    )
endif()

function(rvforge_apply_flags target)
    target_compile_options(${target} PRIVATE ${RVFORGE_C_FLAGS})
    target_compile_definitions(${target} PRIVATE ${RVFORGE_DEFINES})
    target_link_options(${target} PRIVATE ${RVFORGE_LINK_FLAGS})
endfunction()

include_guard(GLOBAL)

function(rvforge_assert_riscv)
    if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
        message(FATAL_ERROR
            "RVForge must be built with the RISC-V toolchain.\n"
            "Pass -DCMAKE_TOOLCHAIN_FILE=toolchain/riscv64-linux-gnu.cmake"
        )
    endif()
endfunction()

function(rvforge_add_qemu_test target)
    cmake_parse_arguments(ARG "" "WORKING_DIRECTORY" "ARGS" ${ARGN})

    find_program(QEMU_BIN qemu-riscv64-static REQUIRED)

    add_test(
        NAME    qemu_${target}
        COMMAND ${QEMU_BIN} -L /usr/riscv64-linux-gnu
                $<TARGET_FILE:${target}>
                ${ARG_ARGS}
        WORKING_DIRECTORY ${ARG_WORKING_DIRECTORY}
    )
    set_tests_properties(qemu_${target} PROPERTIES
        TIMEOUT         120
        PASS_REGULAR_EXPRESSION "STATUS: PASS"
        FAIL_REGULAR_EXPRESSION "STATUS: FAIL;Segmentation fault;Illegal instruction"
    )
endfunction()

function(rvforge_check_no_x86_symbols target_binary output_var)
    find_program(OBJDUMP_BIN riscv64-linux-gnu-objdump REQUIRED)
    set(X86_SYMS "xmm;ymm;zmm;_mm_;" "__SSE;__AVX;__MMX")
    execute_process(
        COMMAND ${OBJDUMP_BIN} -d ${target_binary}
        OUTPUT_VARIABLE DISASM
        ERROR_QUIET
    )
    set(FOUND_X86 FALSE)
    foreach(sym ${X86_SYMS})
        if(DISASM MATCHES "${sym}")
            message(WARNING "x86 symbol '${sym}' found in ${target_binary}")
            set(FOUND_X86 TRUE)
        endif()
    endforeach()
    set(${output_var} ${FOUND_X86} PARENT_SCOPE)
endfunction()

function(rvforge_record_size target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo
            "Binary size: $<TARGET_FILE:${target}>"
        COMMAND riscv64-linux-gnu-size $<TARGET_FILE:${target}>
        VERBATIM
    )
endfunction()

macro(rvforge_external_common_args)
    set(_COMMON_CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${RVFORGE_ROOT}/toolchain/riscv64-linux-gnu.cmake
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=OFF
    )
endmacro()

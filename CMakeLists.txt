cmake_minimum_required(VERSION 3.24)

project(aurex LANGUAGES C CXX)

option(AUREX_ENABLE_COVERAGE "Build Aurex with LLVM source-based coverage instrumentation" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(AUREX_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "AUREX_ENABLE_COVERAGE requires Clang/LLVM for llvm-cov instrumentation")
    endif()
    add_compile_options(-O0 -g -fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate -fcoverage-mapping)
endif()

include(cmake/AurexWarnings.cmake)
include(cmake/AurexBase.cmake)
include(cmake/AurexSyntax.cmake)
include(cmake/AurexFrontend.cmake)
include(cmake/AurexSema.cmake)
include(cmake/AurexLLVM.cmake)
include(cmake/AurexIr.cmake)
include(cmake/AurexBackendLLVM.cmake)
include(cmake/AurexDriver.cmake)
include(cmake/AurexTools.cmake)
include(cmake/AurexTests.cmake)

if(MSVC)
    target_compile_options(aurex_base PRIVATE /W4)
else()
    set(AUREX_WARNING_TARGETS
        aurex_base
        aurex_syntax
        aurex_lex
        aurex_parse
        aurex_sema
        aurex_ir
        aurex_backend_llvm
        aurex_driver
        aurexc
    )
    if(TARGET aurex_lex_bench)
        list(APPEND AUREX_WARNING_TARGETS aurex_lex_bench)
    endif()
    if(TARGET aurex_tests)
        list(APPEND AUREX_WARNING_TARGETS aurex_tests)
    endif()
    if(TARGET aurex_lexer_tests)
        list(APPEND AUREX_WARNING_TARGETS aurex_lexer_tests)
    endif()
    aurex_enable_warnings(${AUREX_WARNING_TARGETS})
endif()

add_custom_target(coverage
    COMMAND "${CMAKE_SOURCE_DIR}/tools/check_coverage.sh"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
)

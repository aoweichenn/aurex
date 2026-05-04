cmake_minimum_required(VERSION 3.24)

project(aurex LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(AUREX_USE_AURORA_BACKEND "Build with Aurora native x86-64 backend" ON)

include(cmake/AurexWarnings.cmake)
include(cmake/AurexBase.cmake)
include(cmake/AurexSyntax.cmake)
include(cmake/AurexFrontend.cmake)
include(cmake/AurexSema.cmake)
include(cmake/AurexLLVM.cmake)
include(cmake/AurexIr.cmake)
include(cmake/AurexBackendLLVM.cmake)

if(AUREX_USE_AURORA_BACKEND)
    set(AURORA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(AURORA_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(AURORA_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_subdirectory(../Aurora ${CMAKE_BINARY_DIR}/aurora EXCLUDE_FROM_ALL)
    include(cmake/AurexBackendAurora.cmake)
else()
    add_library(aurex_backend_aurora
        src/driver/stub_codegen_backend.cpp
    )
    target_link_libraries(aurex_backend_aurora PUBLIC aurex_base aurex_ir)
    target_include_directories(aurex_backend_aurora PUBLIC include)
endif()

include(cmake/AurexDriver.cmake)
include(cmake/AurexTools.cmake)

if(MSVC)
    target_compile_options(aurex_base PRIVATE /W4)
else()
    aurex_enable_warnings(aurex_base aurex_syntax aurex_lex aurex_parse aurex_sema aurex_ir aurex_backend_llvm aurex_backend_aurora aurex_driver aurexc)
endif()

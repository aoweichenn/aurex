cmake_minimum_required(VERSION 3.24)

project(aurex_m0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/AurexWarnings.cmake)
include(cmake/AurexBase.cmake)
include(cmake/AurexSyntax.cmake)
include(cmake/AurexFrontend.cmake)
include(cmake/AurexSema.cmake)
include(cmake/AurexIr.cmake)
include(cmake/AurexCodegenC.cmake)
include(cmake/AurexDriver.cmake)
include(cmake/AurexTools.cmake)

if(MSVC)
    target_compile_options(m0_base PRIVATE /W4)
else()
    aurex_enable_warnings(m0_base m0_syntax m0_lex m0_parse m0_sema m0_ir m0_codegen_c m0_driver m0c)
endif()

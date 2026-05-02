cmake_minimum_required(VERSION 3.24)

project(aurex_m0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(m0_base
    src/base/source.cpp
    src/base/diagnostic.cpp
    src/base/text.cpp
)
target_include_directories(m0_base PUBLIC include)

add_library(m0_syntax
    src/syntax/ast_dump.cpp
    src/syntax/module.cpp
)
target_link_libraries(m0_syntax PUBLIC m0_base)
target_include_directories(m0_syntax PUBLIC include)

add_library(m0_lex
    src/lex/lexer.cpp
)
target_link_libraries(m0_lex PUBLIC m0_base m0_syntax)
target_include_directories(m0_lex PUBLIC include)

add_library(m0_parse
    src/parse/parser.cpp
)
target_link_libraries(m0_parse PUBLIC m0_base m0_syntax)
target_include_directories(m0_parse PUBLIC include)

add_library(m0_sema
    src/sema/type.cpp
    src/sema/symbol.cpp
    src/sema/sema.cpp
)
target_link_libraries(m0_sema PUBLIC m0_base m0_syntax)
target_include_directories(m0_sema PUBLIC include)

add_library(m0_codegen_c
    src/codegen_c/c_type.cpp
    src/codegen_c/expr_order.cpp
    src/codegen_c/c_emitter.cpp
)
target_link_libraries(m0_codegen_c PUBLIC m0_base m0_syntax m0_sema)
target_include_directories(m0_codegen_c PUBLIC include)

add_library(m0_driver
    src/driver/compiler.cpp
    src/driver/module_loader.cpp
)
target_link_libraries(m0_driver PUBLIC m0_base m0_syntax m0_lex m0_parse m0_sema m0_codegen_c)
target_include_directories(m0_driver PUBLIC include)

add_executable(m0c
    src/cli/main.cpp
)
target_link_libraries(m0c PRIVATE m0_driver)

if(MSVC)
    target_compile_options(m0_base PRIVATE /W4)
else()
    foreach(target m0_base m0_syntax m0_lex m0_parse m0_sema m0_codegen_c m0_driver m0c)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()

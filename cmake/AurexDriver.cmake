add_library(aurex_driver
    src/driver/compiler.cpp
    src/driver/module_loader.cpp
    src/driver/native_toolchain.cpp
    src/driver/standard_library.cpp
    src/driver/codegen_backend.cpp
    src/driver/llvm_codegen_backend.cpp
)
target_link_libraries(aurex_driver PUBLIC
    aurex_base aurex_syntax aurex_lex aurex_parse aurex_sema aurex_ir
    aurex_backend_llvm aurex_backend_aurora
)
target_include_directories(aurex_driver PUBLIC include)
target_compile_definitions(aurex_driver PUBLIC
    AUREX_BUILTIN_STDLIB_ROOT=\"${CMAKE_SOURCE_DIR}/std\"
)

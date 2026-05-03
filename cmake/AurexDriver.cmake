add_library(m0_driver
    src/driver/compiler.cpp
    src/driver/module_loader.cpp
    src/driver/native_toolchain.cpp
    src/driver/standard_library.cpp
)
target_link_libraries(m0_driver PUBLIC m0_base m0_syntax m0_lex m0_parse m0_sema m0_ir m0_backend_llvm)
target_include_directories(m0_driver PUBLIC include)
target_compile_definitions(m0_driver PUBLIC
    AUREX_BUILTIN_STDLIB_ROOT=\"${CMAKE_SOURCE_DIR}/std\"
)

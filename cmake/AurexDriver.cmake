add_library(aurex_driver
    src/driver/compiler.cpp
    src/driver/file_cache.cpp
    src/driver/module_loader.cpp
    src/driver/native_toolchain.cpp
)
target_link_libraries(aurex_driver PUBLIC aurex_base aurex_syntax aurex_lex aurex_parse aurex_sema aurex_ir aurex_backend_llvm)
target_include_directories(aurex_driver PUBLIC include)

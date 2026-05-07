add_library(aurex_std_host_c STATIC
    std/ffi/c/support/host_c.c
)
target_include_directories(aurex_std_host_c PRIVATE std/ffi/c/support)

add_library(aurex_driver
    src/driver/compiler.cpp
    src/driver/file_cache.cpp
    src/driver/module_loader.cpp
    src/driver/native_toolchain.cpp
    src/driver/standard_library.cpp
)
add_dependencies(aurex_driver aurex_std_host_c)
target_link_libraries(aurex_driver PUBLIC aurex_base aurex_syntax aurex_lex aurex_parse aurex_sema aurex_ir aurex_backend_llvm)
target_include_directories(aurex_driver PUBLIC include)
target_compile_definitions(aurex_driver PUBLIC
    AUREX_BUILTIN_STDLIB_ROOT=\"${CMAKE_SOURCE_DIR}/std\"
    AUREX_BUILTIN_HOST_C_SUPPORT_LIBRARY=\"$<TARGET_FILE:aurex_std_host_c>\"
)

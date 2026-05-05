add_library(aurex_sema
    src/sema/type.cpp
    src/sema/symbol.cpp
    src/sema/function_registry.cpp
    src/sema/sema.cpp
)
target_link_libraries(aurex_sema PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_sema PUBLIC include)

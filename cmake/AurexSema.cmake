add_library(m0_sema
    src/sema/type.cpp
    src/sema/symbol.cpp
    src/sema/sema.cpp
)
target_link_libraries(m0_sema PUBLIC m0_base m0_syntax)
target_include_directories(m0_sema PUBLIC include)

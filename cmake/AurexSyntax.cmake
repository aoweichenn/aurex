add_library(m0_syntax
    src/syntax/ast_dump.cpp
    src/syntax/module.cpp
)
target_link_libraries(m0_syntax PUBLIC m0_base)
target_include_directories(m0_syntax PUBLIC include)

add_library(m0_codegen_c
    src/codegen_c/c_type.cpp
    src/codegen_c/expr_order.cpp
    src/codegen_c/c_emitter.cpp
)
target_link_libraries(m0_codegen_c PUBLIC m0_base m0_syntax m0_sema)
target_include_directories(m0_codegen_c PUBLIC include)

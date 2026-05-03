add_library(m0_ir
    src/ir/ir.cpp
    src/ir/ir_dump.cpp
    src/ir/lower_ast.cpp
    src/ir/verify.cpp
)
target_link_libraries(m0_ir PUBLIC m0_base m0_syntax m0_sema)
target_include_directories(m0_ir PUBLIC include)

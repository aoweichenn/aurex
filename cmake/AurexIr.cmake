add_library(aurex_ir
    src/ir/enum_layout.cpp
    src/ir/ir.cpp
    src/ir/ir_dump.cpp
    src/ir/lower_ast.cpp
    src/ir/pass_pipeline.cpp
    src/ir/verify.cpp
)
target_link_libraries(aurex_ir PUBLIC aurex_base aurex_syntax aurex_sema)
target_include_directories(aurex_ir PUBLIC include)

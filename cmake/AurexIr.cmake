add_library(aurex_ir
    src/midend/ir/core/analysis_manager.cpp
    src/midend/ir/core/enum_layout.cpp
    src/midend/ir/core/ir.cpp
    src/midend/ir/core/ir_cleanup_marker_facts.cpp
    src/midend/ir/core/ir_dyn_abi_facts.cpp
    src/midend/ir/core/ir_dyn_ownership_runtime_ir_verifier_facts.cpp
    src/midend/ir/core/ir_dump.cpp
    src/midend/ir/core/ir_fingerprint.cpp
    src/midend/ir/core/ir_value_closure.cpp
    src/midend/ir/lowering/sources/lower_ast.cpp
    src/midend/ir/lowering/sources/lower_ast_expr.cpp
    src/midend/ir/lowering/sources/lower_ast_match.cpp
    src/midend/ir/lowering/sources/lower_ast_stmt.cpp
    src/midend/ir/passes/pass_manager.cpp
    src/midend/ir/passes/pass_pipeline.cpp
    src/midend/ir/verify/verify.cpp
)
target_link_libraries(aurex_ir PUBLIC aurex_base aurex_syntax aurex_sema)
target_include_directories(aurex_ir
    PUBLIC include
    PRIVATE src
)

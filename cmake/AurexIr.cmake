find_package(LLVM REQUIRED CONFIG)

add_library(m0_ir
    src/ir/ir.cpp
    src/ir/ir_dump.cpp
    src/ir/llvm_emit.cpp
    src/ir/llvm_emit_function.cpp
    src/ir/llvm_emit_module.cpp
    src/ir/llvm_emit_types.cpp
    src/ir/llvm_emit_util.cpp
    src/ir/llvm_emit_value.cpp
    src/ir/lower_ast.cpp
    src/ir/verify.cpp
)
target_compile_definitions(m0_ir PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(m0_ir SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(m0_ir PUBLIC m0_base m0_syntax m0_sema PRIVATE LLVM)
target_include_directories(m0_ir PUBLIC include)

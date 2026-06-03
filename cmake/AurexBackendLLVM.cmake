add_library(aurex_backend_llvm
    src/backend/llvm/emission/llvm_backend.cpp
    src/backend/llvm/emission/llvm_backend_function.cpp
    src/backend/llvm/emission/llvm_backend_module.cpp
    src/backend/llvm/emission/llvm_backend_types.cpp
    src/backend/llvm/emission/llvm_backend_util.cpp
    src/backend/llvm/emission/llvm_backend_value.cpp
)
target_link_libraries(aurex_backend_llvm PUBLIC aurex_base aurex_ir aurex_llvm)
target_include_directories(aurex_backend_llvm
    PUBLIC include
    PRIVATE src
)

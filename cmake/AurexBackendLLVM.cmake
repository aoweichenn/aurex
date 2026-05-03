add_library(m0_backend_llvm
    src/backend/llvm/llvm_backend.cpp
    src/backend/llvm/llvm_backend_function.cpp
    src/backend/llvm/llvm_backend_module.cpp
    src/backend/llvm/llvm_backend_types.cpp
    src/backend/llvm/llvm_backend_util.cpp
    src/backend/llvm/llvm_backend_value.cpp
)
target_link_libraries(m0_backend_llvm PUBLIC m0_base m0_ir m0_llvm)
target_include_directories(m0_backend_llvm PUBLIC include)

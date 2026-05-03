find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}: ${LLVM_DIR}")

llvm_map_components_to_libnames(AUREX_LLVM_LIBS
    core
    support
    targetparser
    native
)

add_library(m0_llvm INTERFACE)
target_compile_definitions(m0_llvm INTERFACE ${LLVM_DEFINITIONS})
target_include_directories(m0_llvm SYSTEM INTERFACE ${LLVM_INCLUDE_DIRS})
target_link_libraries(m0_llvm INTERFACE ${AUREX_LLVM_LIBS})

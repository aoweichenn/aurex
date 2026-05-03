find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}: ${LLVM_DIR}")

llvm_map_components_to_libnames(AUREX_LLVM_LIBS
    core
    support
    targetparser
    native
)

add_library(aurex_llvm INTERFACE)
target_compile_definitions(aurex_llvm INTERFACE ${LLVM_DEFINITIONS})
target_include_directories(aurex_llvm SYSTEM INTERFACE ${LLVM_INCLUDE_DIRS})
target_link_libraries(aurex_llvm INTERFACE ${AUREX_LLVM_LIBS})

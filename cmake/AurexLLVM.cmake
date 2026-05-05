find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}: ${LLVM_DIR}")

find_program(AUREX_LLVM_CONFIG
    NAMES llvm-config
    HINTS "${LLVM_TOOLS_BINARY_DIR}"
    NO_DEFAULT_PATH
)
if(NOT AUREX_LLVM_CONFIG)
    find_program(AUREX_LLVM_CONFIG NAMES llvm-config)
endif()
if(NOT AUREX_LLVM_CONFIG)
    message(FATAL_ERROR "llvm-config is required to link LLVM without propagating unrelated system include paths")
endif()

execute_process(
    COMMAND "${AUREX_LLVM_CONFIG}" --includedir
    OUTPUT_VARIABLE AUREX_LLVM_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${AUREX_LLVM_CONFIG}" --libfiles core support targetparser native
    OUTPUT_VARIABLE AUREX_LLVM_LIBFILES
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
separate_arguments(AUREX_LLVM_LIBFILES UNIX_COMMAND "${AUREX_LLVM_LIBFILES}")

add_library(aurex_llvm INTERFACE)
target_compile_definitions(aurex_llvm INTERFACE ${LLVM_DEFINITIONS})
target_include_directories(aurex_llvm SYSTEM INTERFACE "${AUREX_LLVM_INCLUDE_DIR}")
target_link_libraries(aurex_llvm INTERFACE ${AUREX_LLVM_LIBFILES})

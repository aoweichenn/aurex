add_library(aurex_driver
    src/driver/backend_pipeline.cpp
    src/driver/cli.cpp
    src/driver/compilation_pipeline.cpp
    src/driver/compilation_session.cpp
    src/driver/compiler.cpp
    src/driver/diagnostic_renderer.cpp
    src/driver/file_cache.cpp
    src/driver/frontend_pipeline.cpp
    src/driver/incremental_cache.cpp
    src/driver/incremental_cache/io.cpp
    src/driver/incremental_cache/profile.cpp
    src/driver/incremental_cache/query.cpp
    src/driver/incremental_cache/query_stats.cpp
    src/driver/incremental_cache/reuse.cpp
    src/driver/incremental_cache/schedule.cpp
    src/driver/incremental_cache/source_stage.cpp
    src/driver/incremental_cache/subjects.cpp
    src/driver/incremental_cache/subjects/module.cpp
    src/driver/incremental_cache/subjects/ordering.cpp
    src/driver/incremental_cache/subjects/semantic.cpp
    src/driver/incremental_cache/subjects/source.cpp
    src/driver/lowering_pipeline.cpp
    src/driver/module_loader.cpp
    src/driver/module_loader_remap.cpp
    src/driver/module_loader_support.cpp
    src/driver/native_toolchain.cpp
    src/driver/pipeline_stage.cpp
    src/driver/profile.cpp
)
target_link_libraries(aurex_driver PUBLIC aurex_base aurex_syntax aurex_lex aurex_parse aurex_sema aurex_ir)
target_include_directories(aurex_driver PUBLIC include)

add_library(aurex_driver_llvm
    src/driver/cli_llvm.cpp
)
target_link_libraries(aurex_driver_llvm PUBLIC aurex_driver PRIVATE aurex_backend_llvm)
target_include_directories(aurex_driver_llvm PUBLIC include)

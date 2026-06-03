add_library(aurex_driver
    src/application/driver/pipeline/sources/backend_pipeline.cpp
    src/application/driver/cli/cli.cpp
    src/application/driver/pipeline/sources/compilation_pipeline.cpp
    src/application/driver/pipeline/sources/compilation_session.cpp
    src/application/driver/core/compiler.cpp
    src/application/driver/diagnostics/diagnostic_renderer.cpp
    src/application/driver/io/file_cache.cpp
    src/application/driver/pipeline/sources/frontend_pipeline.cpp
    src/application/driver/incremental_cache/core/sources/incremental_cache.cpp
    src/application/driver/incremental_cache/io_adapters/sources/io.cpp
    src/application/driver/incremental_cache/query_adapters/sources/profile.cpp
    src/application/driver/incremental_cache/query_adapters/sources/query.cpp
    src/application/driver/incremental_cache/query_adapters/sources/query_stats.cpp
    src/application/driver/incremental_cache/reuse/sources/reuse.cpp
    src/application/driver/incremental_cache/schedule/sources/schedule.cpp
    src/application/driver/incremental_cache/source/sources/source_stage.cpp
    src/application/driver/incremental_cache/schedule/sources/subjects.cpp
    src/application/driver/incremental_cache/subjects/sources/module.cpp
    src/application/driver/incremental_cache/subjects/sources/ordering.cpp
    src/application/driver/incremental_cache/subjects/sources/semantic.cpp
    src/application/driver/incremental_cache/subjects/sources/source.cpp
    src/application/driver/pipeline/sources/lowering_pipeline.cpp
    src/application/driver/modules/sources/module_loader.cpp
    src/application/driver/modules/sources/module_loader_remap.cpp
    src/application/driver/modules/sources/module_loader_support.cpp
    src/application/driver/toolchain/native_toolchain.cpp
    src/application/driver/packages/package_identity.cpp
    src/application/driver/core/project_model.cpp
    src/application/driver/profile/profile.cpp
)
target_link_libraries(aurex_driver PUBLIC aurex_base aurex_project aurex_syntax aurex_lex aurex_parse aurex_sema aurex_ir)
target_link_libraries(aurex_driver PRIVATE
    aurex_pipeline_stage
)
target_include_directories(aurex_driver
    PUBLIC include
    PRIVATE src
)

add_library(aurex_driver_llvm
    src/application/driver/cli/cli_llvm.cpp
)
target_link_libraries(aurex_driver_llvm PUBLIC aurex_driver PRIVATE aurex_backend_llvm)
target_include_directories(aurex_driver_llvm PUBLIC include)

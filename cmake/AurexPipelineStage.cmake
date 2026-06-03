add_library(aurex_pipeline_stage
    src/infrastructure/pipeline/stage.cpp
)
target_link_libraries(aurex_pipeline_stage PUBLIC
    aurex_base
)
target_include_directories(aurex_pipeline_stage PUBLIC
    include
)

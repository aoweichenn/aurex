add_library(aurex_tooling
    src/tooling/ide.cpp
)
target_link_libraries(aurex_tooling PUBLIC
    aurex_base
    aurex_lex
    aurex_parse
    aurex_pipeline_stage
    aurex_query
    aurex_sema
    aurex_syntax
)
target_include_directories(aurex_tooling PUBLIC include)

add_library(aurex_tooling
    src/tooling/ide.cpp
)
target_link_libraries(aurex_tooling PUBLIC
    aurex_base
    aurex_lex
    aurex_parse
    aurex_query
    aurex_sema
    aurex_syntax
)
target_link_libraries(aurex_tooling PRIVATE
    aurex_pipeline_stage
)
target_include_directories(aurex_tooling
    PUBLIC include
    PRIVATE src
)

add_library(aurex_tooling
    src/tooling/ide.cpp
    src/tooling/lsp.cpp
    src/tooling/lsp_stdio.cpp
    src/tooling/reuse.cpp
    src/tooling/session.cpp
    src/tooling/workspace_index.cpp
)
target_link_libraries(aurex_tooling PUBLIC
    aurex_base
    aurex_lex
    aurex_parse
    aurex_pipeline_stage
    aurex_project
    aurex_query
    aurex_sema
    aurex_syntax
)
target_include_directories(aurex_tooling PUBLIC include)

add_executable(aurex-lsp
    src/cli/lsp_main.cpp
)
target_link_libraries(aurex-lsp PRIVATE aurex_tooling)

install(TARGETS aurex-lsp DESTINATION bin)

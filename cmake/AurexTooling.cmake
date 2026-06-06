add_library(aurex_tooling
    src/application/tooling/ide/ide.cpp
    src/application/tooling/lsp/lsp.cpp
    src/application/tooling/lsp/lsp_stdio.cpp
    src/application/tooling/session/reuse.cpp
    src/application/tooling/session/session.cpp
    src/application/tooling/session/workspace_index.cpp
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
if(TARGET aurex_ir)
    target_link_libraries(aurex_tooling PRIVATE aurex_ir)
    target_compile_definitions(aurex_tooling PRIVATE AUREX_TOOLING_ENABLE_IR_FACTS=1)
endif()
target_include_directories(aurex_tooling PUBLIC include)

add_executable(aurex-lsp
    src/application/cli/lsp_main.cpp
)
target_link_libraries(aurex-lsp PRIVATE aurex_tooling)

install(TARGETS aurex-lsp DESTINATION bin)

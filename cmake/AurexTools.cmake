add_executable(aurexc
    src/cli/main.cpp
)
target_link_libraries(aurexc PRIVATE aurex_driver)

add_executable(aurex_lex_bench EXCLUDE_FROM_ALL
    tools/lex_bench.cpp
)
target_link_libraries(aurex_lex_bench PRIVATE aurex_lex)

install(TARGETS aurexc DESTINATION bin)

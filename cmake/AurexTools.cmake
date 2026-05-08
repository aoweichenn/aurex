add_executable(aurexc
    src/cli/main.cpp
)
target_link_libraries(aurexc PRIVATE aurex_driver)

install(TARGETS aurexc DESTINATION bin)

add_executable(aurexc
    src/cli/main.cpp
)
target_link_libraries(aurexc PRIVATE m0_driver)

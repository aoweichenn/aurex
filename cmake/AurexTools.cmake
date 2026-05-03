add_executable(m0c
    src/cli/main.cpp
)
target_link_libraries(m0c PRIVATE m0_driver)

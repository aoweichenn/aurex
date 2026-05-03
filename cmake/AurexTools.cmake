add_executable(aurexc
    src/cli/main.cpp
)
target_link_libraries(aurexc PRIVATE m0_driver)

install(TARGETS aurexc DESTINATION bin)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/std/" DESTINATION share/aurex/std)

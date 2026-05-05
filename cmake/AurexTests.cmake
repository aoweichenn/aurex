include(CTest)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(aurex_tests
        tests/support/test_support.cpp
        tests/aurex_integration_tests.cpp
        tests/m1/m1_control_flow_tests.cpp
        tests/m1/m1_enum_tests.cpp
        tests/m1/m1_expression_tests.cpp
        tests/m1/m1_integration_tests.cpp
    )
    target_link_libraries(aurex_tests PRIVATE GTest::gtest_main)
    target_include_directories(aurex_tests PRIVATE tests)
    target_compile_definitions(aurex_tests PRIVATE
        AUREX_TEST_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
        AUREX_TEST_BINARY_DIR=\"${CMAKE_BINARY_DIR}\"
        AUREX_TEST_CMAKE_COMMAND=\"${CMAKE_COMMAND}\"
    )
    set_target_properties(aurex_tests PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:GTest::gtest_main>"
    )

    add_test(NAME aurex_tests COMMAND aurex_tests --gtest_color=auto)
    set_tests_properties(aurex_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

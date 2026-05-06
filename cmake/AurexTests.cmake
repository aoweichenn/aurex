include(CTest)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(aurex_tests
        tests/support/test_support.cpp
        tests/gtest/backend/llvm_constants_tests.cpp
        tests/gtest/backend/llvm_runtime_tests.cpp
        tests/gtest/backend/llvm_utility_tests.cpp
        tests/gtest/base/base_tests.cpp
        tests/gtest/driver/cli_argument_tests.cpp
        tests/gtest/driver/cli_driver_tests.cpp
        tests/gtest/driver/install_and_import_tests.cpp
        tests/gtest/driver/native_toolchain_tests.cpp
        tests/gtest/driver/standard_library_tests.cpp
        tests/gtest/frontend/ast_dump_tests.cpp
        tests/gtest/frontend/lexer_tests.cpp
        tests/gtest/frontend/parser_tests.cpp
        tests/gtest/integration/documentation_tests.cpp
        tests/gtest/integration/examples_tests.cpp
        tests/gtest/integration/native_execution_tests.cpp
        tests/gtest/integration/regression_tests.cpp
        tests/gtest/integration/sample_suite_tests.cpp
        tests/gtest/ir/ir_dump_tests.cpp
        tests/gtest/ir/ir_verifier_edge_tests.cpp
        tests/gtest/ir/ir_verifier_structural_tests.cpp
        tests/gtest/ir/lower_ast_whitebox_tests.cpp
        tests/gtest/ir/pass_pipeline_tests.cpp
        tests/gtest/ir/type_table_tests.cpp
        tests/gtest/sema/block_expression_tests.cpp
        tests/gtest/sema/error_handling_tests.cpp
        tests/gtest/sema/functions_tests.cpp
        tests/gtest/sema/generics_tests.cpp
        tests/gtest/sema/if_expression_tests.cpp
        tests/gtest/sema/inference_tests.cpp
        tests/gtest/sema/modules_visibility_tests.cpp
        tests/gtest/sema/pattern_matching_tests.cpp
        tests/gtest/sema/sema_whitebox_tests.cpp
        tests/gtest/sema/type_alias_tests.cpp
    )
    target_link_libraries(aurex_tests PRIVATE
        GTest::gtest_main
        aurex_base
        aurex_lex
        aurex_ir
        aurex_backend_llvm
        aurex_driver
    )
    target_include_directories(aurex_tests PRIVATE tests)
    target_compile_definitions(aurex_tests PRIVATE
        AUREX_TEST_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
        AUREX_TEST_BINARY_DIR=\"${CMAKE_BINARY_DIR}\"
        AUREX_TEST_CMAKE_COMMAND=\"${CMAKE_COMMAND}\"
    )
    set_target_properties(aurex_tests PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:GTest::gtest_main>"
    )

    add_test(
        NAME aurex_tests
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=-AurexIntegrationTest.SampleSuite_*
    )
    add_test(
        NAME aurex_tests_sample_suite_positive
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_PositiveSamples
    )
    add_test(
        NAME aurex_tests_sample_suite_negative
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_NegativeSamples
    )
    add_test(
        NAME aurex_tests_sample_suite_std_bootstrap
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_Std_std_bootstrap
    )
    add_test(
        NAME aurex_tests_sample_suite_std_collections_path
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_Std_std_collections_path
    )
    add_test(
        NAME aurex_tests_sample_suite_std_ffi
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_Std_std_ffi
    )
    add_test(
        NAME aurex_tests_sample_suite_std_file
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_Std_std_file
    )
    add_test(
        NAME aurex_tests_sample_suite_std_mem
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_Std_std_mem
    )
    add_test(
        NAME aurex_tests_sample_suite_std_text
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_Std_std_text
    )
    set_tests_properties(aurex_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(
        aurex_tests_sample_suite_positive
        aurex_tests_sample_suite_negative
        aurex_tests_sample_suite_std_bootstrap
        aurex_tests_sample_suite_std_collections_path
        aurex_tests_sample_suite_std_ffi
        aurex_tests_sample_suite_std_file
        aurex_tests_sample_suite_std_mem
        aurex_tests_sample_suite_std_text
        PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        LABELS "slow;sample-suite"
        TIMEOUT 300
    )
endif()

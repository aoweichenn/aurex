include(CTest)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(aurex_base_tests
        tests/gtest/base/base_tests.cpp
    )
    target_link_libraries(aurex_base_tests PRIVATE
        GTest::gtest_main
        aurex_base
    )
    set_target_properties(aurex_base_tests PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:GTest::gtest_main>"
    )
    add_test(
        NAME aurex_tests_base_unit
        COMMAND aurex_base_tests --gtest_color=auto
    )

    add_executable(aurex_query_tests
        tests/gtest/query/query_key_tests.cpp
        tests/gtest/query/query_robustness_tests.cpp
    )
    target_link_libraries(aurex_query_tests PRIVATE
        GTest::gtest_main
        aurex_query
    )
    set_target_properties(aurex_query_tests PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:GTest::gtest_main>"
    )
    add_test(
        NAME aurex_tests_query_unit
        COMMAND aurex_query_tests --gtest_color=auto
    )

    add_executable(aurex_lexer_tests
        tests/gtest/frontend/lexer_tests.cpp
    )
    target_link_libraries(aurex_lexer_tests PRIVATE
        GTest::gtest_main
        aurex_lex
    )
    target_include_directories(aurex_lexer_tests PRIVATE
        src
    )
    set_target_properties(aurex_lexer_tests PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:GTest::gtest_main>"
    )
    add_test(
        NAME aurex_tests_lexer_unit
        COMMAND aurex_lexer_tests --gtest_color=auto
    )

    add_executable(aurex_frontend_tests
        tests/gtest/base/base_tests.cpp
        tests/gtest/frontend/ast_dump_tests.cpp
        tests/gtest/frontend/lexer_tests.cpp
        tests/gtest/frontend/lossless_syntax_tests.cpp
        tests/gtest/frontend/parser_tests.cpp
        tests/gtest/sema/sema_whitebox_tests.cpp
        tests/gtest/tooling/ide_tooling_tests.cpp
    )
    target_link_libraries(aurex_frontend_tests PRIVATE
        GTest::gtest_main
        aurex_base
        aurex_lex
        aurex_parse
        aurex_sema
        aurex_tooling
    )
    target_include_directories(aurex_frontend_tests PRIVATE
        tests
        src
    )
    target_compile_definitions(aurex_frontend_tests PRIVATE
        AUREX_SEMA_WHITEBOX_TESTS=1
    )
    set_target_properties(aurex_frontend_tests PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:GTest::gtest_main>"
    )
    add_test(
        NAME aurex_tests_frontend_only
        COMMAND aurex_frontend_tests --gtest_color=auto
    )

    if(NOT AUREX_FRONTEND_ONLY)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)

        add_executable(aurex_tests
            tests/support/test_support.cpp
            tests/gtest/backend/llvm_constants_tests.cpp
            tests/gtest/backend/llvm_runtime_tests.cpp
            tests/gtest/backend/llvm_types_whitebox_tests.cpp
            tests/gtest/backend/llvm_utility_tests.cpp
            tests/gtest/base/base_tests.cpp
            tests/gtest/driver/cli_argument_tests.cpp
            tests/gtest/driver/cli_driver_tests.cpp
            tests/gtest/driver/install_and_import_tests.cpp
            tests/gtest/driver/native_toolchain_tests.cpp
            tests/gtest/frontend/ast_dump_tests.cpp
            tests/gtest/frontend/randomized_frontend_tests.cpp
            tests/gtest/frontend/lexer_tests.cpp
            tests/gtest/frontend/lossless_syntax_tests.cpp
            tests/gtest/frontend/parser_tests.cpp
            tests/gtest/integration/documentation_tests.cpp
            tests/gtest/integration/examples_tests.cpp
            tests/gtest/integration/native_execution_tests.cpp
            tests/gtest/integration/randomized_integration_tests.cpp
            tests/gtest/integration/regression_tests.cpp
            tests/gtest/integration/sample_suite_tests.cpp
            tests/gtest/ir/analysis_manager_tests.cpp
            tests/gtest/ir/ir_dump_tests.cpp
            tests/gtest/ir/ir_verifier_edge_tests.cpp
            tests/gtest/ir/ir_verifier_structural_tests.cpp
            tests/gtest/ir/lower_ast_whitebox_tests.cpp
            tests/gtest/ir/pass_pipeline_tests.cpp
            tests/gtest/ir/type_table_tests.cpp
            tests/gtest/query/query_key_tests.cpp
            tests/gtest/query/query_robustness_tests.cpp
            tests/gtest/sema/block_expression_tests.cpp
            tests/gtest/sema/error_handling_tests.cpp
            tests/gtest/sema/functions_tests.cpp
            tests/gtest/sema/if_expression_tests.cpp
            tests/gtest/sema/inference_tests.cpp
            tests/gtest/sema/modules_visibility_tests.cpp
            tests/gtest/sema/pattern_matching_tests.cpp
            tests/gtest/sema/sema_whitebox_tests.cpp
            tests/gtest/sema/type_alias_tests.cpp
            tests/gtest/tooling/ide_tooling_tests.cpp
        )
        target_link_libraries(aurex_tests PRIVATE
            GTest::gtest_main
            aurex_base
            aurex_lex
            aurex_ir
            aurex_backend_llvm
            aurex_driver
            aurex_driver_llvm
            aurex_tooling
        )
        target_include_directories(aurex_tests PRIVATE
            tests
            src
        )
        target_compile_definitions(aurex_tests PRIVATE
            AUREX_SEMA_WHITEBOX_TESTS=1
            AUREX_TEST_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
            AUREX_TEST_BINARY_DIR=\"${CMAKE_BINARY_DIR}\"
            AUREX_TEST_CMAKE_COMMAND=\"${CMAKE_COMMAND}\"
        )
        set_target_properties(aurex_tests PROPERTIES
            BUILD_RPATH "$<TARGET_FILE_DIR:GTest::gtest_main>"
        )

        function(aurex_add_gtest name filter)
            add_test(
                NAME "${name}"
                COMMAND aurex_tests --gtest_color=auto "--gtest_filter=${filter}"
            )
        endfunction()

        aurex_add_gtest(aurex_tests_core_unit
            "CoreUnit.*"
        )
        aurex_add_gtest(aurex_tests_query_unit_full
            "QueryUnit.*"
        )
        aurex_add_gtest(aurex_tests_driver_and_regressions
            "AurexIntegrationTest.Cli*:AurexIntegrationTest.Compiler*:AurexIntegrationTest.IncrementalCache*:AurexIntegrationTest.InstallAndImportPaths:AurexIntegrationTest.ModuleLoader*:AurexIntegrationTest.DocumentationLayoutIsStable:AurexIntegrationTest.Examples*:AurexIntegrationTest.NativeHello*:AurexIntegrationTest.StructAndEnumValidationRegressions:AurexIntegrationTest.Diagnostic*:AurexIntegrationTest.IntegerLiteralRegressions:AurexIntegrationTest.M2UnsafeBoundaries:AurexIntegrationTest.M2SafeReferences:AurexIntegrationTest.StringCheckedBoundary:AurexIntegrationTest.ArrayLiteralRegressions:AurexIntegrationTest.SliceRegressions:AurexIntegrationTest.TupleRegressions:AurexIntegrationTest.EnumConstructorMatchArmRegressions:AurexIntegrationTest.EnumAdtRegressions:AurexIntegrationTest.QualifiedStaticMethodRegressions:AurexIntegrationTest.MainAndCliRegressions:AurexIntegrationTest.SymlinkedImportStillValidatesExpectedModuleName:AurexIntegrationTest.M2Generic*:AurexIntegrationTest.Randomized*"
        )
        aurex_add_gtest(aurex_tests_functions
            "AurexIntegrationTest.BlockExpression:AurexIntegrationTest.TryExpression*:AurexIntegrationTest.FunctionPrototypes:AurexIntegrationTest.MultiParameterFunctionAcceptsCIdentifier:AurexIntegrationTest.VariadicExternCFunctions:AurexIntegrationTest.AbiCollisionDiagnosticsIncludePreviousDeclaration:AurexIntegrationTest.FunctionTypesAndIndirectCalls:AurexIntegrationTest.PublicFunctionsRequireExplicitReturnType:AurexIntegrationTest.DeferScopes:AurexIntegrationTest.ForStatementAndValueSemantics:AurexIntegrationTest.RecursiveFunctions:AurexIntegrationTest.MethodsAndAssociatedFunctions"
        )
        aurex_add_gtest(aurex_tests_control_and_modules
            "AurexIntegrationTest.IfExpression:AurexIntegrationTest.LocalTypeInference:AurexIntegrationTest.FunctionReturnInference:AurexIntegrationTest.ModuleVisibility:AurexIntegrationTest.PublicImportReexport:AurexIntegrationTest.DefaultPrivateVisibility"
        )
        aurex_add_gtest(aurex_tests_pattern_and_types
            "AurexIntegrationTest.MatchExpression:AurexIntegrationTest.EnumPayloadAndMatchBinding:AurexIntegrationTest.MatchWildcardAndScopedCases:AurexIntegrationTest.StructuralMatchExhaustiveness:AurexIntegrationTest.MatchOrPattern:AurexIntegrationTest.PatternRemainingSliceLetElseAndBindingOr:AurexIntegrationTest.MatchLiteralPattern:AurexIntegrationTest.MatchGuard:AurexIntegrationTest.LayoutAlignment:AurexIntegrationTest.TypeAlias"
        )
        add_test(
            NAME aurex_tests_sample_suite_positive
            COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_Positive*
        )
        add_test(
            NAME aurex_tests_sample_suite_negative
            COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.SampleSuite_Negative*
        )
        set_tests_properties(
            aurex_tests_core_unit
            aurex_tests_query_unit_full
            aurex_tests_driver_and_regressions
            aurex_tests_functions
            aurex_tests_control_and_modules
            aurex_tests_pattern_and_types
            PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
        set_tests_properties(
            aurex_tests_sample_suite_positive
            PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            LABELS "slow;sample-suite"
            TIMEOUT 300
        )
        set_tests_properties(
            aurex_tests_sample_suite_negative
            PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            LABELS "slow;sample-suite"
            TIMEOUT 300
        )
        if(AUREX_ENABLE_REGEX_CONFORMANCE)
            add_test(
                NAME aurex_regex_differential_conformance
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/regex_differential.py"
            )
            set_tests_properties(
                aurex_regex_differential_conformance
                PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                LABELS "slow;regex;conformance"
                TIMEOUT 900
            )
        endif()
    endif()

    if(AUREX_BUILD_FUZZERS AND NOT AUREX_FRONTEND_ONLY)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "AUREX_BUILD_FUZZERS requires Clang for -fsanitize=fuzzer")
        endif()

        add_executable(aurex_frontend_fuzzer
            tests/fuzz/frontend_fuzzer.cpp
        )
        target_link_libraries(aurex_frontend_fuzzer PRIVATE
            aurex_base
            aurex_lex
            aurex_parse
            aurex_sema
            aurex_ir
        )
        target_include_directories(aurex_frontend_fuzzer PRIVATE
            tests
        )
        target_compile_options(aurex_frontend_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_options(aurex_frontend_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()
endif()

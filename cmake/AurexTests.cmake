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

    function(aurex_add_gtest name filter)
        add_test(
            NAME "${name}"
            COMMAND aurex_tests --gtest_color=auto "--gtest_filter=${filter}"
        )
    endfunction()

    aurex_add_gtest(aurex_tests_core_unit
        "CoreUnit.*"
    )
    aurex_add_gtest(aurex_tests_driver_and_regressions
        "AurexIntegrationTest.Cli*:AurexIntegrationTest.Compiler*:AurexIntegrationTest.InstallAndImportPaths:AurexIntegrationTest.DocumentationLayoutIsStable:AurexIntegrationTest.SystemExamples*:AurexIntegrationTest.ExamplesDocumentationAndLibrariesArePresent:AurexIntegrationTest.NativeHelloOutputs:AurexIntegrationTest.StructAndEnumValidationRegressions:AurexIntegrationTest.IntegerLiteralRegressions:AurexIntegrationTest.GenericEnumConstructorMatchArmRegressions:AurexIntegrationTest.QualifiedGenericStaticMethodRegressions:AurexIntegrationTest.MainAndCliRegressions:AurexIntegrationTest.SymlinkedImportStillValidatesExpectedModuleName:AurexIntegrationTest.StdTextSampleExposesGenericSpanBaseline"
    )
    aurex_add_gtest(aurex_tests_functions
        "AurexIntegrationTest.BlockExpression:AurexIntegrationTest.TryExpression*:AurexIntegrationTest.FunctionPrototypes:AurexIntegrationTest.VariadicExternCFunctions:AurexIntegrationTest.DeferScopes:AurexIntegrationTest.RecursiveFunctions:AurexIntegrationTest.MethodsAndAssociatedFunctions"
    )
    aurex_add_gtest(aurex_tests_generics
        "AurexIntegrationTest.GenericEnumOption:AurexIntegrationTest.GenericEnumResultExpectedType:AurexIntegrationTest.GenericEnumDiagnostics:AurexIntegrationTest.GenericStructPair:AurexIntegrationTest.GenericStructLiteralInference:AurexIntegrationTest.GenericFunctionIdentity:AurexIntegrationTest.GenericFunctionImport:AurexIntegrationTest.GenericImplMethods:AurexIntegrationTest.QualifiedGenericSubstitutionImport:AurexIntegrationTest.QualifiedGenericInferenceUsesAliasScope:AurexIntegrationTest.GenericStructArrayFieldAndSmallPayloadEnum:AurexIntegrationTest.GenericStructDiagnostics:AurexIntegrationTest.GenericFunctionDiagnostics:AurexIntegrationTest.GenericImportVisibilityAndAmbiguityDiagnostics"
    )
    aurex_add_gtest(aurex_tests_control_and_modules
        "AurexIntegrationTest.IfExpression:AurexIntegrationTest.LocalTypeInference:AurexIntegrationTest.FunctionReturnInference:AurexIntegrationTest.ModuleVisibility:AurexIntegrationTest.PublicImportReexport"
    )
    aurex_add_gtest(aurex_tests_pattern_and_types
        "AurexIntegrationTest.MatchExpression:AurexIntegrationTest.EnumPayloadAndMatchBinding:AurexIntegrationTest.MatchWildcardAndScopedCases:AurexIntegrationTest.MatchOrPattern:AurexIntegrationTest.MatchLiteralPattern:AurexIntegrationTest.MatchGuard:AurexIntegrationTest.LayoutAlignment:AurexIntegrationTest.TypeAlias"
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
    add_test(
        NAME aurex_tests_std_collections_path_baseline
        COMMAND aurex_tests --gtest_color=auto --gtest_filter=AurexIntegrationTest.StdCollectionsPathSampleExposesM1ContainerBaseline
    )
    set_tests_properties(
        aurex_tests_core_unit
        aurex_tests_driver_and_regressions
        aurex_tests_functions
        aurex_tests_generics
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
        RUN_SERIAL TRUE
    )
    set_tests_properties(
        aurex_tests_sample_suite_negative
        aurex_tests_sample_suite_std_bootstrap
        aurex_tests_sample_suite_std_ffi
        aurex_tests_sample_suite_std_file
        aurex_tests_sample_suite_std_mem
        aurex_tests_sample_suite_std_text
        PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        LABELS "slow;sample-suite"
        TIMEOUT 300
    )
    set_tests_properties(
        aurex_tests_std_collections_path_baseline
        PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        LABELS "slow;std-baseline"
        TIMEOUT 300
    )
endif()

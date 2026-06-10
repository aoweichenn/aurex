include(CTest)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(aurex_base_tests
        tests/gtest/infrastructure/base/base_tests.cpp
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
        tests/gtest/infrastructure/query/const_generic_design_gate_tests.cpp
        tests/gtest/infrastructure/query/dyn_advanced_design_gate_tests.cpp
        tests/gtest/infrastructure/query/dyn_ownership_runtime_boundary_gate_tests.cpp
        tests/gtest/infrastructure/query/dyn_ownership_runtime_facts_tests.cpp
        tests/gtest/infrastructure/query/dyn_ownership_runtime_ir_verifier_facts_tests.cpp
        tests/gtest/infrastructure/query/owned_dyn_ir_shape_prototype_gate_tests.cpp
        tests/gtest/infrastructure/query/owned_dyn_runtime_admission_gate_tests.cpp
        tests/gtest/infrastructure/query/principal_set_composition_facts_tests.cpp
        tests/gtest/infrastructure/query/query_key_tests.cpp
        tests/gtest/infrastructure/query/query_robustness_tests.cpp
        tests/gtest/infrastructure/query/trait_object_upcast_key_tests.cpp
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
        tests/gtest/frontend/lex/lexer_tests.cpp
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
        tests/gtest/infrastructure/base/base_tests.cpp
        tests/gtest/frontend/syntax/ast_dump_tests.cpp
        tests/gtest/frontend/lex/lexer_tests.cpp
        tests/gtest/frontend/syntax/lossless_syntax_tests.cpp
        tests/gtest/frontend/parse/parser_tests.cpp
        tests/gtest/frontend/parse/trait_supertrait_parse_tests.cpp
        tests/gtest/frontend/sema/identifier_tests.cpp
        tests/gtest/frontend/sema/lifetime_tests.cpp
        tests/gtest/frontend/sema/move_rejection_facts_tests.cpp
        tests/gtest/frontend/sema/place_state_tests.cpp
        tests/gtest/frontend/sema/sema_borrow_whitebox_tests.cpp
        tests/gtest/frontend/sema/sema_expressions_patterns_whitebox_tests.cpp
        tests/gtest/frontend/sema/sema_generics_whitebox_tests.cpp
        tests/gtest/frontend/sema/sema_lookup_modules_whitebox_tests.cpp
        tests/gtest/frontend/sema/sema_resources_dropck_whitebox_tests.cpp
        tests/gtest/frontend/sema/sema_types_whitebox_tests.cpp
        tests/gtest/frontend/sema/sema_whitebox_tests.cpp
        tests/gtest/frontend/sema/statement_tests.cpp
        tests/gtest/frontend/sema/dyn_trait_composition_tests.cpp
        tests/gtest/frontend/sema/dyn_trait_composition_supertrait_tests.cpp
        tests/gtest/frontend/sema/dyn_trait_whitebox_tests.cpp
        tests/gtest/frontend/sema/dyn_trait_upcast_tests.cpp
        tests/gtest/frontend/sema/trait_supertrait_facts_tests.cpp
        tests/gtest/application/tooling/ide_tooling_tests.cpp
        tests/gtest/application/tooling/session_lsp_tooling_tests.cpp
    )
    target_link_libraries(aurex_frontend_tests PRIVATE
        GTest::gtest_main
        aurex_tooling
    )
    target_include_directories(aurex_frontend_tests PRIVATE
        tests
        src
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
            tests/gtest/backend/llvm/llvm_constants_tests.cpp
            tests/gtest/backend/llvm/llvm_runtime_tests.cpp
            tests/gtest/backend/llvm/llvm_types_whitebox_tests.cpp
            tests/gtest/backend/llvm/llvm_utility_tests.cpp
            tests/gtest/infrastructure/base/base_tests.cpp
            tests/gtest/application/driver/cli_argument_tests.cpp
            tests/gtest/application/driver/cli_driver_tests.cpp
            tests/gtest/application/driver/install_and_import_tests.cpp
            tests/gtest/application/driver/native_toolchain_tests.cpp
            tests/gtest/application/driver/project_model_tests.cpp
            tests/gtest/frontend/syntax/ast_dump_tests.cpp
            tests/gtest/frontend/parse/randomized_frontend_tests.cpp
            tests/gtest/frontend/lex/lexer_tests.cpp
            tests/gtest/frontend/syntax/lossless_syntax_tests.cpp
            tests/gtest/frontend/parse/parser_tests.cpp
            tests/gtest/frontend/parse/trait_supertrait_parse_tests.cpp
            tests/gtest/integration/documentation_tests.cpp
            tests/gtest/integration/examples_tests.cpp
            tests/gtest/integration/native_execution_tests.cpp
            tests/gtest/integration/randomized_integration_tests.cpp
            tests/gtest/integration/regression_tests.cpp
            tests/gtest/integration/sample_suite_tests.cpp
            tests/gtest/midend/ir/analysis_manager_tests.cpp
            tests/gtest/midend/ir/ir_dump_tests.cpp
            tests/gtest/midend/ir/ir_fingerprint_tests.cpp
            tests/gtest/midend/ir/ir_verifier_edge_tests.cpp
            tests/gtest/midend/ir/ir_verifier_structural_tests.cpp
            tests/gtest/midend/ir/lower_ast_aggregate_rollback_tests.cpp
            tests/gtest/midend/ir/lower_ast_dyn_trait_tests.cpp
            tests/gtest/midend/ir/lower_ast_whitebox_tests.cpp
            tests/gtest/midend/ir/owned_dyn_ir_shape_prototype_tests.cpp
            tests/gtest/midend/ir/pass_pipeline_tests.cpp
            tests/gtest/midend/ir/type_table_tests.cpp
            tests/gtest/infrastructure/query/query_key_tests.cpp
            tests/gtest/infrastructure/query/const_generic_design_gate_tests.cpp
            tests/gtest/infrastructure/query/dyn_advanced_design_gate_tests.cpp
            tests/gtest/infrastructure/query/dyn_ownership_runtime_boundary_gate_tests.cpp
            tests/gtest/infrastructure/query/dyn_ownership_runtime_facts_tests.cpp
            tests/gtest/infrastructure/query/dyn_ownership_runtime_ir_verifier_facts_tests.cpp
            tests/gtest/infrastructure/query/owned_dyn_ir_shape_prototype_gate_tests.cpp
            tests/gtest/infrastructure/query/owned_dyn_runtime_admission_gate_tests.cpp
            tests/gtest/infrastructure/query/principal_set_composition_facts_tests.cpp
            tests/gtest/infrastructure/query/query_robustness_tests.cpp
            tests/gtest/infrastructure/query/trait_object_upcast_key_tests.cpp
            tests/gtest/frontend/sema/block_expression_tests.cpp
            tests/gtest/frontend/sema/error_handling_tests.cpp
            tests/gtest/frontend/sema/functions_tests.cpp
            tests/gtest/frontend/sema/identifier_tests.cpp
            tests/gtest/frontend/sema/if_expression_tests.cpp
            tests/gtest/frontend/sema/inference_tests.cpp
            tests/gtest/frontend/sema/lifetime_tests.cpp
            tests/gtest/frontend/sema/move_rejection_facts_tests.cpp
            tests/gtest/frontend/sema/place_state_tests.cpp
            tests/gtest/frontend/sema/modules_visibility_tests.cpp
            tests/gtest/frontend/sema/pattern_matching_tests.cpp
            tests/gtest/frontend/sema/sema_borrow_whitebox_tests.cpp
            tests/gtest/frontend/sema/sema_expressions_patterns_whitebox_tests.cpp
            tests/gtest/frontend/sema/sema_generics_whitebox_tests.cpp
            tests/gtest/frontend/sema/sema_lookup_modules_whitebox_tests.cpp
            tests/gtest/frontend/sema/sema_resources_dropck_whitebox_tests.cpp
            tests/gtest/frontend/sema/sema_types_whitebox_tests.cpp
            tests/gtest/frontend/sema/sema_whitebox_tests.cpp
            tests/gtest/frontend/sema/statement_tests.cpp
            tests/gtest/frontend/sema/dyn_trait_composition_tests.cpp
            tests/gtest/frontend/sema/dyn_trait_composition_supertrait_tests.cpp
            tests/gtest/frontend/sema/dyn_trait_whitebox_tests.cpp
            tests/gtest/frontend/sema/dyn_trait_upcast_tests.cpp
            tests/gtest/frontend/sema/trait_tests.cpp
            tests/gtest/frontend/sema/trait_supertrait_facts_tests.cpp
            tests/gtest/frontend/sema/type_alias_tests.cpp
            tests/gtest/application/tooling/ide_tooling_tests.cpp
            tests/gtest/application/tooling/session_lsp_tooling_tests.cpp
        )
        target_link_libraries(aurex_tests PRIVATE
            GTest::gtest_main
            aurex_driver_llvm
            aurex_tooling
        )
        # Backend whitebox tests include LLVM headers directly.  The LLVM backend
        # itself remains a link-only dependency through aurex_driver_llvm.
        target_compile_definitions(aurex_tests PRIVATE
            $<TARGET_PROPERTY:aurex_llvm,INTERFACE_COMPILE_DEFINITIONS>
        )
        target_include_directories(aurex_tests PRIVATE
            tests
            src
        )
        target_include_directories(aurex_tests SYSTEM PRIVATE
            $<TARGET_PROPERTY:aurex_llvm,INTERFACE_SYSTEM_INCLUDE_DIRECTORIES>
        )
        target_compile_definitions(aurex_tests PRIVATE
            AUREX_TEST_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
            AUREX_TEST_BINARY_DIR=\"${CMAKE_BINARY_DIR}\"
            AUREX_TEST_CMAKE_COMMAND=\"${CMAKE_COMMAND}\"
        )
        if(TARGET aurexc)
            add_dependencies(aurex_tests aurexc)
        endif()
        if(TARGET aurex-lsp)
            add_dependencies(aurex_tests aurex-lsp)
        endif()
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
            "AurexIntegrationTest.Cli*:AurexIntegrationTest.Compiler*:AurexIntegrationTest.IncrementalCache*:AurexIntegrationTest.InstallAndImportPaths:AurexIntegrationTest.ModuleLoader*:AurexIntegrationTest.DocumentationLayoutIsStable:AurexIntegrationTest.M4ReleaseDocumentationIsClosed:AurexIntegrationTest.M5ReleaseDocumentationIsClosed:AurexIntegrationTest.M6ResourceSemanticsWp2ThroughWp7AreClosed:AurexIntegrationTest.M8DynTraitDesignDocumentationIsCurrent:AurexIntegrationTest.M9DynAbiToolingDesignDocumentationIsCurrent:AurexIntegrationTest.Examples*:AurexIntegrationTest.NativeHello*:AurexIntegrationTest.StructAndEnumValidationRegressions:AurexIntegrationTest.Diagnostic*:AurexIntegrationTest.IntegerLiteralRegressions:AurexIntegrationTest.M2UnsafeBoundaries:AurexIntegrationTest.M2SafeReferences:AurexIntegrationTest.StringCheckedBoundary:AurexIntegrationTest.ArrayLiteralRegressions:AurexIntegrationTest.SliceRegressions:AurexIntegrationTest.TupleRegressions:AurexIntegrationTest.EnumConstructorMatchArmRegressions:AurexIntegrationTest.EnumAdtRegressions:AurexIntegrationTest.QualifiedStaticMethodRegressions:AurexIntegrationTest.MainAndCliRegressions:AurexIntegrationTest.SymlinkedImportStillValidatesExpectedModuleName:AurexIntegrationTest.M2Generic*:AurexIntegrationTest.Randomized*"
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
        aurex_add_gtest(aurex_tests_visibility_borrow_remaining
            "AurexIntegrationTest.ExportedSignatureSurfacesRejectPrivateTypes:AurexIntegrationTest.M2NestedGenericInstantiationRegressions:AurexIntegrationTest.M7bBlockExpressionPrecheckScansInternalStatements:AurexIntegrationTest.M7bBorrowContractsUseDeclaredCalleeBoundaries:AurexIntegrationTest.M7bReturnedBorrowViewsParticipateInLocalLoanChecking:AurexIntegrationTest.M7dBorrowEscapeFallbackCoversPatternStorageGuard:AurexIntegrationTest.ModulePartImportsDoNotLeakAcrossPrimaryOrParts:AurexIntegrationTest.ModulePartPrivateItemsStayHiddenFromExternalModules:AurexIntegrationTest.ModulePartPublicImportsDoNotBecomeModuleReexports:AurexIntegrationTest.ModulePartsSharePrivateModuleSurface:AurexIntegrationTest.ModulePartsUsePartLocalImportsAndSharedItems:AurexIntegrationTest.PackageVisibilityControlsPackageReexports:AurexIntegrationTest.PackageVisibilityControlsPackageSurface:AurexIntegrationTest.PackageVisibilitySurfaceLeaksUseVisibilityAwareDiagnostics:AurexIntegrationTest.PublicMethodsOnPrivateTypesAreNotExportedSurfaces:AurexIntegrationTest.SelectiveUseReexport:AurexIntegrationTest.SelectiveUseReexportGenericItems:AurexIntegrationTest.TraitImplRegistrySamples"
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
            aurex_tests_visibility_borrow_remaining
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

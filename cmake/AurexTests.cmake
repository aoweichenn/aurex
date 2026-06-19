include(CTest)
include("${CMAKE_CURRENT_LIST_DIR}/AurexTestSources.cmake")

if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(aurex_base_tests
        ${AUREX_BASE_TEST_SOURCES}
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
        ${AUREX_QUERY_TEST_SOURCES}
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
        ${AUREX_LEXER_TEST_SOURCES}
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
        ${AUREX_FRONTEND_ONLY_TEST_SOURCES}
    )
    target_link_libraries(aurex_frontend_tests PRIVATE
        GTest::gtest_main
        aurex_macro
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
            ${AUREX_FULL_TEST_SOURCES}
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

        function(aurex_add_gtest name)
            set(one_value_args COST TIMEOUT)
            set(multi_value_args LABELS PATTERNS)
            cmake_parse_arguments(AUREX_GTEST "" "${one_value_args}" "${multi_value_args}" ${ARGN})
            if(AUREX_GTEST_UNPARSED_ARGUMENTS)
                message(FATAL_ERROR "unexpected aurex_add_gtest arguments: ${AUREX_GTEST_UNPARSED_ARGUMENTS}")
            endif()
            if(NOT AUREX_GTEST_PATTERNS)
                message(FATAL_ERROR "aurex_add_gtest(${name}) requires at least one PATTERNS entry")
            endif()
            list(JOIN AUREX_GTEST_PATTERNS ":" filter)
            add_test(
                NAME "${name}"
                COMMAND aurex_tests --gtest_color=auto "--gtest_filter=${filter}"
            )
            set_tests_properties("${name}" PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            )
            if(AUREX_GTEST_COST)
                set_tests_properties("${name}" PROPERTIES
                    COST "${AUREX_GTEST_COST}"
                )
            endif()
            if(AUREX_GTEST_LABELS)
                set_tests_properties("${name}" PROPERTIES
                    LABELS "${AUREX_GTEST_LABELS}"
                )
            endif()
            if(AUREX_GTEST_TIMEOUT)
                set_tests_properties("${name}" PROPERTIES
                    TIMEOUT "${AUREX_GTEST_TIMEOUT}"
                )
            endif()
        endfunction()

        aurex_add_gtest(aurex_tests_core_unit
            PATTERNS
            "CoreUnit.*"
        )
        aurex_add_gtest(aurex_tests_query_unit_full
            PATTERNS
            "QueryUnit.*"
        )
        aurex_add_gtest(aurex_tests_docs_current
            PATTERNS
            "AurexIntegrationTest.CurrentChineseDocumentationLayoutIsStable"
            "AurexIntegrationTest.CurrentDocumentationExplainsTheActiveRoadmap"
            "AurexIntegrationTest.SyntaxRevisionDocumentationMatchesCurrentSurface"
            LABELS docs
        )
        aurex_add_gtest(aurex_tests_cli_driver
            PATTERNS
            "AurexIntegrationTest.Cli*"
            "AurexIntegrationTest.Compiler*"
        )
        aurex_add_gtest(aurex_tests_incremental_cache_core
            PATTERNS
            "AurexIntegrationTest.IncrementalCacheWritesValidatesInvalidatesAndReusesCheck"
            "AurexIntegrationTest.IncrementalCacheReportsWriteOpenFailure"
            "AurexIntegrationTest.IncrementalCachePipelineReportsWriteFailureAtEmitStopPoints"
            "AurexIntegrationTest.IncrementalCacheWritesQueryRowsInDependencyScheduleOrder"
            "AurexIntegrationTest.IncrementalCacheParsesQueryDependencyEdgeRows"
            "AurexIntegrationTest.IncrementalCacheRejectsMalformedMismatchedAndBlockedCacheFiles"
        )
        aurex_add_gtest(aurex_tests_incremental_cache_pruning
            PATTERNS
            "AurexIntegrationTest.IncrementalCacheQueryPruning*"
            "AurexIntegrationTest.IncrementalCacheWritesGeneric*"
            "AurexIntegrationTest.IncrementalCacheRecordsTraitDefaultMethodInstanceRows"
            "AurexIntegrationTest.IncrementalCacheLowerIRRowsUseOptimizedIrUnitFingerprint"
        )
        aurex_add_gtest(aurex_tests_incremental_cache_modules
            PATTERNS
            "AurexIntegrationTest.IncrementalCacheItemSignature*"
            "AurexIntegrationTest.IncrementalCacheModule*"
            "AurexIntegrationTest.IncrementalCacheKeys*"
            "AurexIntegrationTest.IncrementalCacheReusesUnusedImportPathManifestSourceRoot"
            "AurexIntegrationTest.IncrementalCacheRejectsImportPathManifestIdentityChanges"
            "AurexIntegrationTest.IncrementalCacheSemanticSubjectsUseModuleIdPackagesForSplitLogicalNames"
            "AurexIntegrationTest.IncrementalCacheTracksImportPathAndDependencyFingerprints"
        )
        aurex_add_gtest(aurex_tests_module_loader_core
            PATTERNS
            "AurexIntegrationTest.InstallAndImportPaths"
            "AurexIntegrationTest.ModuleLoaderRemapsExpressionPayloadsWithoutFatNodes"
            "AurexIntegrationTest.ModuleLoaderLoadsPrimarySidecarParts"
            "AurexIntegrationTest.ModuleLoaderRecordsModulePartKeys"
            "AurexIntegrationTest.ModuleLoaderDiscoversOwningPrimaryForPartCheck"
            "AurexIntegrationTest.ModuleLoaderDiagnostics*"
            "AurexIntegrationTest.ModuleLoaderRejectsAmbiguousImportCandidates"
            "AurexIntegrationTest.ModuleLoaderRejectsDuplicateItemsAcrossParts"
        )
        aurex_add_gtest(aurex_tests_module_loader_packages
            PATTERNS
            "AurexIntegrationTest.ModuleLoaderImportVisibilityChangesGraphAndExportsOnly"
            "AurexIntegrationTest.ModuleLoaderSelectiveUseReexportChangesGraphAndExportsOnly"
            "AurexIntegrationTest.ModuleLoaderPackageImportVisibilityChangesPackageExportsOnly"
            "AurexIntegrationTest.ModuleLoaderAssignsImportPathModulesToDistinctPackages"
            "AurexIntegrationTest.ModuleLoaderUses*"
            "AurexIntegrationTest.ModuleLoaderRejectsManifestSourceRootPathTopologyMismatches"
            "AurexIntegrationTest.ModuleLoaderPackageManifestIdentityHandlesSyntaxAndFallbacks"
        )
        aurex_add_gtest(aurex_tests_project_model
            PATTERNS
            "AurexIntegrationTest.ModuleLoaderProjectModel*"
            "AurexIntegrationTest.IncrementalCacheSourceStageRecordsSupportLosslessToolingMode"
        )
        aurex_add_gtest(aurex_tests_examples_core
            PATTERNS
            "AurexIntegrationTest.ExamplesHello*"
            "AurexIntegrationTest.ExamplesCommon*"
            "AurexIntegrationTest.ExamplesDocumentation*"
            "AurexIntegrationTest.ExamplesDiagnostic*"
            LABELS examples
        )
        aurex_add_gtest(aurex_tests_examples_regex_basic
            PATTERNS
            "AurexIntegrationTest.ExamplesRegexLibrary*"
            "AurexIntegrationTest.ExamplesRegexPhase1*"
            LABELS slow examples regex
            TIMEOUT 300
            COST 25
        )
        aurex_add_gtest(aurex_tests_examples_regex_advanced
            PATTERNS
            "AurexIntegrationTest.ExamplesRegexIndustrial*"
            "AurexIntegrationTest.ExamplesRegexAdvanced*"
            LABELS slow examples regex
            TIMEOUT 300
            COST 30
        )
        aurex_add_gtest(aurex_tests_examples_regex_stress
            PATTERNS
            "AurexIntegrationTest.ExamplesRegexStress*"
            "AurexIntegrationTest.ExamplesRegexRejectsOversizedPattern"
            LABELS slow examples regex
            TIMEOUT 300
            COST 20
        )
        aurex_add_gtest(aurex_tests_native_hello
            PATTERNS
            "AurexIntegrationTest.NativeHello*"
            LABELS native
        )
        aurex_add_gtest(aurex_tests_native_dyn_traits
            PATTERNS
            "AurexIntegrationTest.NativeDefaultAndNamedArgumentsUseCheckedOrder"
            "AurexIntegrationTest.NativeDynTrait*"
            "AurexIntegrationTest.NativeMutableDynTrait*"
            LABELS native
        )
        aurex_add_gtest(aurex_tests_randomized_driver
            PATTERNS
            "AurexIntegrationTest.Randomized*"
            LABELS randomized
        )
        aurex_add_gtest(aurex_tests_regressions_core
            PATTERNS
            "AurexIntegrationTest.StructAndEnumValidationRegressions"
            "AurexIntegrationTest.Diagnostic*"
            "AurexIntegrationTest.IntegerLiteralRegressions"
            "AurexIntegrationTest.M2UnsafeBoundaries"
            "AurexIntegrationTest.M2SafeReferences"
            "AurexIntegrationTest.StringCheckedBoundary"
            "AurexIntegrationTest.ArrayLiteralRegressions"
            "AurexIntegrationTest.SliceRegressions"
            "AurexIntegrationTest.TupleRegressions"
            "AurexIntegrationTest.EnumConstructorMatchArmRegressions"
            "AurexIntegrationTest.EnumAdtRegressions"
            "AurexIntegrationTest.QualifiedStaticMethodRegressions"
            "AurexIntegrationTest.MainAndCliRegressions"
            "AurexIntegrationTest.SymlinkedImportStillValidatesExpectedModuleName"
        )
        aurex_add_gtest(aurex_tests_regressions_generics
            PATTERNS
            "AurexIntegrationTest.M2Generic*"
            "AurexIntegrationTest.BuiltinDeriveCapability*"
        )
        aurex_add_gtest(aurex_tests_struct_field_references
            PATTERNS
            "AurexIntegrationTest.StructFieldReferences*"
        )
        aurex_add_gtest(aurex_tests_functions
            PATTERNS
            "AurexIntegrationTest.BlockExpression"
            "AurexIntegrationTest.TryExpression*"
            "AurexIntegrationTest.FunctionPrototypes"
            "AurexIntegrationTest.MultiParameterFunctionAcceptsCIdentifier"
            "AurexIntegrationTest.VariadicExternCFunctions"
            "AurexIntegrationTest.AbiCollisionDiagnosticsIncludePreviousDeclaration"
            "AurexIntegrationTest.FunctionTypesAndIndirectCalls"
            "AurexIntegrationTest.PublicFunctionsRequireExplicitReturnType"
            "AurexIntegrationTest.DeferScopes"
            "AurexIntegrationTest.ForStatementAndValueSemantics"
            "AurexIntegrationTest.RecursiveFunctions"
            "AurexIntegrationTest.MethodsAndAssociatedFunctions"
        )
        aurex_add_gtest(aurex_tests_control_and_modules
            PATTERNS
            "AurexIntegrationTest.IfExpression"
            "AurexIntegrationTest.LocalTypeInference"
            "AurexIntegrationTest.FunctionReturnInference"
            "AurexIntegrationTest.ModuleVisibility"
            "AurexIntegrationTest.PublicImportReexport"
            "AurexIntegrationTest.DefaultPrivateVisibility"
        )
        aurex_add_gtest(aurex_tests_pattern_and_types
            PATTERNS
            "AurexIntegrationTest.MatchExpression"
            "AurexIntegrationTest.EnumPayloadAndMatchBinding"
            "AurexIntegrationTest.MatchWildcardAndScopedCases"
            "AurexIntegrationTest.StructuralMatchExhaustiveness"
            "AurexIntegrationTest.MatchOrPattern"
            "AurexIntegrationTest.PatternRemainingSliceLetElseAndBindingOr"
            "AurexIntegrationTest.MatchLiteralPattern"
            "AurexIntegrationTest.MatchGuard"
            "AurexIntegrationTest.LayoutAlignment"
            "AurexIntegrationTest.TypeAlias"
        )
        aurex_add_gtest(aurex_tests_visibility_borrow_remaining
            PATTERNS
            "AurexIntegrationTest.ExportedSignatureSurfacesRejectPrivateTypes"
            "AurexIntegrationTest.M2NestedGenericInstantiationRegressions"
            "AurexIntegrationTest.M7bBlockExpressionPrecheckScansInternalStatements"
            "AurexIntegrationTest.M7bBorrowContractsUseDeclaredCalleeBoundaries"
            "AurexIntegrationTest.M7bReturnedBorrowViewsParticipateInLocalLoanChecking"
            "AurexIntegrationTest.M7dBorrowEscapeFallbackCoversPatternStorageGuard"
            "AurexIntegrationTest.ModulePartImportsDoNotLeakAcrossPrimaryOrParts"
            "AurexIntegrationTest.ModulePartPrivateItemsStayHiddenFromExternalModules"
            "AurexIntegrationTest.ModulePartPublicImportsDoNotBecomeModuleReexports"
            "AurexIntegrationTest.ModulePartsSharePrivateModuleSurface"
            "AurexIntegrationTest.ModulePartsUsePartLocalImportsAndSharedItems"
            "AurexIntegrationTest.PackageVisibilityControlsPackageReexports"
            "AurexIntegrationTest.PackageVisibilityControlsPackageSurface"
            "AurexIntegrationTest.PackageVisibilitySurfaceLeaksUseVisibilityAwareDiagnostics"
            "AurexIntegrationTest.PublicMethodsOnPrivateTypesAreNotExportedSurfaces"
            "AurexIntegrationTest.SelectiveUseReexport"
            "AurexIntegrationTest.SelectiveUseReexportGenericItems"
            "AurexIntegrationTest.TraitImplRegistrySamples"
        )
        aurex_add_gtest(aurex_tests_sample_suite_positive_static
            PATTERNS
            "AurexIntegrationTest.SampleSuite_PositiveSamples"
            LABELS slow sample-suite
            TIMEOUT 300
            COST 5
        )
        aurex_add_gtest(aurex_tests_sample_suite_runtime_core
            PATTERNS
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_pointer*"
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_eval*"
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_const_binary"
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_builtins"
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_compound_assignment"
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_block_expression"
            LABELS sample-suite
        )
        aurex_add_gtest(aurex_tests_sample_suite_runtime_lambdas
            PATTERNS
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_lambda*"
            LABELS sample-suite
        )
        aurex_add_gtest(aurex_tests_sample_suite_runtime_for
            PATTERNS
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_for*"
            LABELS sample-suite
        )
        aurex_add_gtest(aurex_tests_sample_suite_runtime_types_patterns
            PATTERNS
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_tuple*"
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_str*"
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_pattern*"
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_const_pattern"
            LABELS sample-suite
        )
        aurex_add_gtest(aurex_tests_sample_suite_runtime_generics
            PATTERNS
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_generic*"
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_method_local*"
            LABELS sample-suite
        )
        aurex_add_gtest(aurex_tests_sample_suite_runtime_traits
            PATTERNS
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_trait*"
            LABELS sample-suite
        )
        aurex_add_gtest(aurex_tests_sample_suite_runtime_imports
            PATTERNS
            "AurexIntegrationTest.SampleSuite_PositiveRuntime_imported_samples"
            LABELS sample-suite
            COST 2
        )
        aurex_add_gtest(aurex_tests_sample_suite_negative_diagnostics
            PATTERNS
            "AurexIntegrationTest.SampleSuite_NegativeSamples"
            LABELS slow sample-suite
            TIMEOUT 300
            COST 8
        )
        aurex_add_gtest(aurex_tests_sample_suite_negative_cross_stage
            PATTERNS
            "AurexIntegrationTest.SampleSuite_NegativeCrossStageSamples"
            LABELS sample-suite
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

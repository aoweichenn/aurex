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

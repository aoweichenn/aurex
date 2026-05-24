add_library(aurex_sema
    src/sema/type.cpp
    src/sema/canonical_type_builder.cpp
    src/sema/symbol.cpp
    src/sema/identifier.cpp
    src/sema/function.cpp
    src/sema/function_registry.cpp
    src/sema/generic.cpp
    src/sema/generic_instance_identity.cpp
    src/sema/checked_module.cpp
    src/sema/internal/name_resolution.cpp
    src/sema/internal/sema_builtin_expression_analyzer.cpp
    src/sema/internal/sema_diagnostics.cpp
    src/sema/internal/sema_expression_analyzer.cpp
    src/sema/internal/sema_lookup_resolver.cpp
    src/sema/internal/sema_pipeline.cpp
    src/sema/internal/sema_side_tables.cpp
    src/sema/internal/sema_type_services.cpp
    src/sema/match.cpp
    src/sema/sema_call.cpp
    src/sema/sema_decls.cpp
    src/sema/sema_expr.cpp
    src/sema/sema_lookup.cpp
    src/sema/sema_record.cpp
    src/sema/sema_stmt.cpp
    src/sema/sema_types.cpp
    src/sema/sema_facade.cpp
    src/sema/sema.cpp
)
target_link_libraries(aurex_sema PUBLIC aurex_base aurex_syntax aurex_query)
target_include_directories(aurex_sema
    PUBLIC include
    PRIVATE src
)

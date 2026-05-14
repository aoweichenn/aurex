add_library(aurex_sema
    src/sema/type.cpp
    src/sema/symbol.cpp
    src/sema/function_registry.cpp
    src/sema/generic.cpp
    src/sema/checked_module.cpp
    src/sema/match.cpp
    src/sema/sema_call.cpp
    src/sema/sema_decls.cpp
    src/sema/sema_expr.cpp
    src/sema/sema_lookup.cpp
    src/sema/sema_postfix.cpp
    src/sema/sema_record.cpp
    src/sema/sema_stmt.cpp
    src/sema/sema_types.cpp
    src/sema/sema.cpp
)
target_link_libraries(aurex_sema PUBLIC aurex_base aurex_syntax)
target_include_directories(aurex_sema PUBLIC include)

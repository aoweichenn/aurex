add_library(aurex_query
    src/query/stable_hash.cpp
    src/query/stable_identity.cpp
    src/query/query_key.cpp
    src/query/query_interner.cpp
    src/query/stable_key_decoder.cpp
    src/query/query_edge_verifier.cpp
    src/query/query_result.cpp
    src/query/query_reuse.cpp
    src/query/source_file_query.cpp
    src/query/diagnostics_query.cpp
    src/query/module_graph_query.cpp
    src/query/module_exports_query.cpp
    src/query/item_list_query.cpp
    src/query/item_signature_query.cpp
    src/query/generic_template_signature_query.cpp
    src/query/generic_instance_signature_query.cpp
    src/query/generic_instance_body_query.cpp
    src/query/lower_function_ir_query.cpp
    src/query/function_body_syntax_query.cpp
    src/query/type_check_body_query.cpp
    src/query/query_context.cpp
    src/query/canonical_type_key.cpp
    src/query/generic_instance_key.cpp
)
target_link_libraries(aurex_query PUBLIC aurex_base)
target_include_directories(aurex_query PUBLIC include)

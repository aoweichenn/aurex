add_library(aurex_query
    src/query/stable_hash.cpp
    src/query/stable_identity.cpp
    src/query/query_key.cpp
    src/query/query_result.cpp
    src/query/canonical_type_key.cpp
    src/query/generic_instance_key.cpp
)
target_link_libraries(aurex_query PUBLIC aurex_base)
target_include_directories(aurex_query PUBLIC include)

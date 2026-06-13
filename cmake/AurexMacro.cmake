add_library(aurex_macro
    src/frontend/macro/sources/early_item_expansion.cpp
    src/frontend/macro/sources/output_contract/collector.cpp
    src/frontend/macro/sources/output_contract/dump.cpp
    src/frontend/macro/sources/output_contract/fingerprint.cpp
    src/frontend/macro/sources/output_contract/identity.cpp
    src/frontend/macro/sources/output_contract/summary.cpp
    src/frontend/macro/sources/output_contract/validation.cpp
)
target_link_libraries(aurex_macro PUBLIC aurex_base aurex_syntax aurex_query)
target_include_directories(aurex_macro
    PUBLIC include
    PRIVATE src
)

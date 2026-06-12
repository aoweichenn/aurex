add_library(aurex_macro
    src/frontend/macro/sources/early_item_expansion.cpp
)
target_link_libraries(aurex_macro PUBLIC aurex_base aurex_syntax aurex_query)
target_include_directories(aurex_macro
    PUBLIC include
    PRIVATE src
)

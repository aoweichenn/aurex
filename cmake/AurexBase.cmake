add_library(aurex_base
    src/base/bump_allocator.cpp
    src/base/source.cpp
    src/base/diagnostic.cpp
    src/base/string_literal.cpp
    src/base/text.cpp
)
target_include_directories(aurex_base PUBLIC include)

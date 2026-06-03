add_library(aurex_base
    src/infrastructure/base/bump_allocator.cpp
    src/infrastructure/base/source.cpp
    src/infrastructure/base/diagnostic.cpp
    src/infrastructure/base/string_literal.cpp
    src/infrastructure/base/text.cpp
)
target_include_directories(aurex_base PUBLIC include)

add_library(m0_base
    src/base/source.cpp
    src/base/diagnostic.cpp
    src/base/text.cpp
)
target_include_directories(m0_base PUBLIC include)

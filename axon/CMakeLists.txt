cmake_minimum_required(VERSION 3.20)
project(nex VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Library (core) ───────────────────────────────────────────────────────

set(NEX_SOURCES
    src/config.cpp
    src/scanner.cpp
    src/graph.cpp
    src/digest.cpp
    src/cache.cpp
    src/executor.cpp
)

set(NEX_HEADERS
    include/nex/error.hpp
    include/nex/config.hpp
    include/nex/scanner.hpp
    include/nex/graph.hpp
    include/nex/digest.hpp
    include/nex/cache.hpp
    include/nex/executor.hpp
)

add_library(nex_core STATIC ${NEX_SOURCES} ${NEX_HEADERS})
target_include_directories(nex_core PUBLIC include)
target_compile_options(nex_core PRIVATE -Wall -Wextra -Wpedantic)

# ── Executable ────────────────────────────────────────────────────────────

add_executable(nex_bin src/main.cpp)
target_link_libraries(nex_bin PRIVATE nex_core)
set_target_properties(nex_bin PROPERTIES OUTPUT_NAME nex)

# ── Install ───────────────────────────────────────────────────────────────

install(TARGETS nex_bin DESTINATION bin)

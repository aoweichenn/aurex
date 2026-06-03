add_executable(aurexc
    src/application/cli/main.cpp
)
target_link_libraries(aurexc PRIVATE aurex_driver_llvm)

add_executable(aurex_lex_bench EXCLUDE_FROM_ALL
    tools/lex_bench.cpp
)
target_link_libraries(aurex_lex_bench PRIVATE aurex_lex)

if(AUREX_BUILD_BENCHMARKS)
    find_package(benchmark CONFIG QUIET)
    if(NOT benchmark_FOUND)
        include(FetchContent)
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            google_benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.9.5
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(google_benchmark)
    endif()

    if(TARGET benchmark::benchmark)
        set(AUREX_GOOGLE_BENCHMARK_TARGET benchmark::benchmark)
    elseif(TARGET benchmark)
        set(AUREX_GOOGLE_BENCHMARK_TARGET benchmark)
    else()
        message(FATAL_ERROR "Google Benchmark target benchmark::benchmark was not found")
    endif()

    add_executable(aurex_frontend_bench EXCLUDE_FROM_ALL
        tools/frontend_bench.cpp
    )
    target_link_libraries(aurex_frontend_bench PRIVATE
        aurex_lex
        aurex_parse
        aurex_sema
        ${AUREX_GOOGLE_BENCHMARK_TARGET}
    )

    add_executable(aurex_frontend_compare_bench EXCLUDE_FROM_ALL
        tools/frontend_compare_bench.cpp
    )
    target_link_libraries(aurex_frontend_compare_bench PRIVATE
        ${AUREX_GOOGLE_BENCHMARK_TARGET}
    )
endif()

install(TARGETS aurexc DESTINATION bin)

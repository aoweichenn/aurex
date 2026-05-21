# Lexer Performance Notes

This document records how to measure lexer-only performance without mixing in
parser, driver, backend, shell script, or test-process startup costs.

## Targets

Build only the lexer benchmark when measuring tokenization:

```sh
cmake --build build/full-llvm --target aurex_lex_bench -j
```

Build and run the isolated lexer unit tests when checking behavior:

```sh
cmake --build build/full-llvm --target aurex_lexer_tests -j
build/full-llvm/bin/aurex_lexer_tests --gtest_color=auto
ctest --test-dir build/full-llvm -R aurex_tests_lexer_unit --output-on-failure
```

Do not run multiple CMake builds or benchmark processes against the same build
directory at the same time. Parallel runs can contend on CPU and can also race
while relinking `aurex_lex`.

## Benchmark Inputs

The benchmark keeps the original positional form:

```sh
build/full-llvm/bin/aurex_lex_bench <iterations> <repetitions> <scenario>
```

Supported synthetic scenarios:

```text
mixed
identifiers
numbers
strings
punctuation
```

Use `--file` for real source files. The file contents are repeated by the
`repetitions` argument, so the benchmark still runs on a large enough buffer:

```sh
build/full-llvm/bin/aurex_lex_bench 300 128 --file examples/libs/common/result.ax --warmup 50 --runs 5
```

Use `--warmup` to run unmeasured iterations before collecting timing. Use
`--runs` to collect multiple measured runs. The output reports min, median, max,
and average for elapsed time, nanoseconds per byte, and nanoseconds per token.

## Current Baseline

Representative local results after the lexer hot-path work:

```text
build/full-llvm/bin/aurex_lex_bench 500 64 mixed --warmup 100 --runs 5
ns_per_byte_median: 42.2582
ns_per_token_median: 119.534

build/full-llvm/bin/aurex_lex_bench 500 256 punctuation --warmup 100 --runs 5
ns_per_byte_median: 38.6846
ns_per_token_median: 90.9515

build/full-llvm/bin/aurex_lex_bench 300 128 --file examples/libs/common/result.ax --warmup 50 --runs 5
ns_per_byte_median: 37.4493
ns_per_token_median: 132.355
```

Treat these as local comparison points, not absolute cross-machine numbers.
Keep or revert future micro-optimizations based on repeated benchmark runs, not
single-run noise.

---
name: cpp-project-standards
description: Global mandatory skill for all C++ work. Load and apply whenever creating, editing, reviewing, analyzing, refactoring, optimizing, or designing C++ code, including class member access style, responsibility boundaries, function decomposition, constants, modern C++20 usage, iterative algorithms instead of recursion, unreachable/dead-code elimination, standard-library-style API design, include/CMake conventions, design pattern judgment, research-backed design decisions, deep project analysis, mandatory testing and coverage, post-change review, and performance regression prevention.
metadata:
  short-description: User C++ coding standards
---

# C++ Project Standards

This is a global mandatory skill. Use it whenever working on C++ code for the user, including implementation, refactoring, review, project analysis, architecture/design discussion, language/compiler design, algorithm choice, performance optimization, detailed local changes, and test updates.

## Required Standards

1. **Explicit member access**
   - Inside class methods, access all class data members and member functions with `this->`.
   - Apply this consistently to reads, writes, method calls, and constructor/helper logic where the member belongs to the current object.

2. **Focused class and function responsibility**
   - Keep each class responsible for one coherent domain concept or module role.
   - Avoid classes that accumulate unrelated behavior or excessive public/private methods.
   - When a class or function grows too broad, prefer extracting cohesive collaborators, value objects, strategy objects, visitors, builders, or free functions as appropriate for the surrounding codebase.

3. **Function decomposition**
   - Do not place too much complex logic inside one function.
   - Split large functions into cohesive, clearly named helper functions, private methods, local callable objects, small collaborating classes, or standard-library/ranges algorithms when that improves readability and testability.
   - Separate orchestration from detailed steps: parsing/validation, state updates, error handling, transformation, emission, cleanup, and reporting should not be tangled in one long block.
   - Use design patterns, language features, or data structures to reduce complexity only when they make the decomposition clearer and reduce coupling.
   - When editing an oversized function, refactor the touched logic into smaller units as part of the change whenever technically possible.
   - Avoid over-splitting trivial straight-line logic; prefer decomposition when the function has multiple phases, nested branching, repeated logic, hidden side effects, or independent testable decisions.

4. **No magic numbers**
   - Do not leave unexplained numeric literals, string literals, character codes, bit masks, limits, or sentinel values inline when they encode meaning.
   - Replace them with named constants.
   - Constants must use uppercase names.
   - Name constants by combining module/context and purpose, for example `LEXER_MAX_TOKEN_LENGTH`, `PARSER_DEFAULT_LOOKAHEAD`, or `IR_INVALID_REGISTER_ID`.
   - Local one-off literals that are inherently obvious and conventional, such as `0`, `1`, `nullptr`, or empty container initialization, may remain inline when they do not encode domain meaning.

5. **Prefer modern C++**
   - Prefer C++20-era language and library features when they make code clearer, safer, or more expressive.
   - Consider `std::string_view`, `std::span`, ranges, structured bindings, `std::optional`, `std::variant`, `std::expected` when available in the toolchain, concepts, `constexpr`, `consteval`, `enum class`, smart pointers, RAII, and standard containers before custom machinery.
   - Preserve compatibility with the project's configured compiler, standard library, and build settings.

6. **Avoid avoidable copies**
   - Do not pass, return, bind, or capture non-trivial objects by value when the code only observes them.
   - Prefer `const T&` for read-only access to existing objects, `T&` for mutation, `std::span` for contiguous sequences, and `std::string_view`/project interned text views for non-owning text.
   - Pass by value only when the callee intentionally takes ownership, must make a copy, needs a local independently mutable value, is accepting a cheap scalar/handle/iterator/view, or uses a deliberate move/sink API. Make that intent clear with naming, `std::move`, or API shape.
   - In range-for loops and structured bindings, use `const auto&` or `auto&` for elements whose type is not cheap to copy; avoid `auto` copies of AST nodes, IR records, diagnostics, symbols, type info, vectors, strings, paths, maps, and other payload objects on hot compiler paths.
   - Lambda captures should capture large read-only objects by reference, not by value, unless lifetime or async execution requires ownership.
   - When reviewing or analyzing C++ code, report avoidable copies as performance and maintainability issues, especially in parser, AST, sema, IR lowering, diagnostics, and driver hot paths.

7. **No recursion**
   - Do not keep recursive or mutually recursive runtime algorithms in C++ code.
   - Implement all traversals and repeated processing with iterative approaches such as explicit stacks, queues, worklists, loops, state machines, or standard algorithms.
   - This applies especially to AST/IR/tree traversal, graph traversal, parser recovery walks, module dependency traversal, filesystem traversal, and optimization/dataflow worklists.
   - If existing code is recursive and the task touches the affected module, refactor the recursion to an iterative form as part of the work whenever technically possible.
   - When reviewing or analyzing code, report existing recursion as a standards violation and propose an iterative replacement.
   - Only allow recursion when it is required by an external API or unavoidable for a narrowly scoped compile-time metaprogramming case; document the reason and bound the depth.

8. **Unreachable and dead code elimination**
   - Do not keep unreachable code, dead branches, dead stores, unused fallback paths, obsolete feature paths, or logic that can never execute under the current invariants.
   - Before removing or rewriting unreachable code, analyze why it is unreachable: valid invariant, stale implementation, impossible state, missing caller, disabled feature, over-broad guard, or latent bug.
   - Optimize away the unreachable/dead code after the cause is understood, and update nearby invariants, comments, diagnostics, tests, or documentation when the old code implied behavior that no longer exists.
   - If the unreachable path exposes an actual bug or missing test coverage, fix the root cause and add tests rather than only deleting the branch.
   - When review or static analysis finds unreachable code, report the cause and the cleanup strategy explicitly.

9. **Header include and CMake conventions**
   - Prefer angle-bracket includes for all headers, including project headers, after the appropriate include roots are defined in CMake.
   - Configure include roots with target-scoped CMake commands such as `target_include_directories`, using `PRIVATE`, `PUBLIC`, or `INTERFACE` visibility correctly.
   - Include project headers by stable module paths, for example `#include <lexer/token.hpp>` rather than fragile relative paths.
   - Avoid ad hoc quoted includes and deep relative includes such as `#include "../../..."` unless integrating a local file that intentionally should not be exposed through an include root.
   - Keep include paths explicit, minimal, and target-local so modules do not accidentally depend on unrelated directories.
   - Keep implementation directories decomposed by responsibility. A directory named `internal/` must be an implementation root with role-specific subdirectories, not a flat file bucket; do not add new files directly under `internal/`.
   - Mirror decomposition in CMake targets and tests where it improves ownership clarity: facts, collectors, solvers, enforcers, diagnostics, adapters, passes, and backend-specific emitters should live in separate subdirectories or targets when the module is nontrivial.

10. **Reduce coupling with appropriate design patterns**
   - Use design patterns only when they solve a concrete coupling, ownership, lifecycle, construction, traversal, or variability problem.
   - Good candidates include Strategy for interchangeable behavior, Visitor for AST/IR traversal, Builder/Factory for complex construction, Observer/Event-style boundaries for decoupled notifications, and Adapter/Facade for external or unstable interfaces.
   - Do not introduce a pattern only to make the code look more formal. Prefer the simplest design that keeps module boundaries clear.
   - Avoid service locators, global mutable registries, large inheritance hierarchies, or generic "manager" classes when explicit dependencies, small value objects, builders, strategies, or adapters are clearer.
   - For compiler analyses, prefer explicit fact collection, solving, enforcement, diagnostic formatting, and tooling projection boundaries over one analyzer class that owns every phase.

11. **Standard-library-style API design**
    - When designing language/library features, runtime APIs, containers, algorithms, iterators, utilities, or public module interfaces, align naming, semantics, overload shape, iterator/range behavior, error signaling, ownership, and complexity expectations with the C++ standard library where practical.
    - Prefer familiar C++ vocabulary and conventions such as `begin`, `end`, `size`, `empty`, `find`, `contains`, `insert`, `erase`, `push_back`, `emplace`, `value`, `has_value`, `operator*`, `operator->`, RAII, iterator categories, ranges, and value/reference semantics when they fit.
    - Make APIs predictable for C++ users: const-correctness, noexcept/constexpr where appropriate, clear ownership, minimal surprising side effects, documented complexity, and compatibility with standard algorithms/ranges.
    - It is acceptable to learn from other languages when they have a better design for the specific problem, but adapt the idea into idiomatic C++ instead of copying foreign naming or semantics blindly.
    - When borrowing from another language, explain what is being borrowed, why it is better for this case, what C++ convention it replaces or extends, and any tradeoffs.

12. **Research-backed design decisions**
    - For design-oriented tasks, do not provide a shallow proposal. First analyze the existing related project features, language constraints, algorithms, data structures, module boundaries, and performance requirements.
    - Search current research, mature open-source implementations, compiler/language references, and industrial-grade solutions when the design depends on nontrivial algorithms, architecture, or programming-language design tradeoffs.
    - Compare candidate designs by motivation, advantages, disadvantages, complexity, performance, maintainability, integration cost, and fit for the user's actual scenario.
    - For programming language or compiler design, explicitly explain the design motivation, semantic model, algorithmic choice, known alternatives, tradeoffs, and why the selected approach is appropriate here.
    - Include detailed analysis of relevant references instead of merely listing them. Make clear which ideas are adopted, rejected, or adapted for this project.
    - Choose the best practical algorithm/design for the project context, not merely the newest or most academically interesting one.

13. **Deep project analysis**
    - For codebase analysis, detailed local modification, refactoring, or performance optimization tasks, do not inspect only a few obvious files and stop.
    - Study all relevant code and data under the current project scope before making claims or changes. Use file search, symbol search, call-site search, build/test configuration, and existing tests to map the affected area.
    - Analyze module structure, ownership boundaries, data flow, control flow, APIs, invariants, error paths, memory/lifetime behavior, and performance hot paths relevant to the task.
    - Identify strengths, weaknesses, risks, hidden coupling, duplication, scalability limits, and likely maintenance problems in the current implementation.
    - For fine-grained changes, trace every important caller/callee and affected data structure so the change fits the existing behavior instead of only satisfying the local file.
    - For performance work, establish the current algorithmic complexity, allocation/copy behavior, repeated work, cache/locality concerns, and practical measurement options before choosing an optimization.
    - Summarize the analysis concretely, with file/function references when useful, before or alongside the proposed change.

14. **Mandatory testing and coverage**
    - All code changes must be tested when testing is technically possible. Do not treat untested code as complete.
    - Tests must include coverage reporting when the project has, or can reasonably support, a coverage workflow.
    - New code must reach at least 95% coverage. Existing code affected by the change should also be brought to at least 95% coverage when practical within the task scope.
    - For hard-to-reach branches, error paths, dependency boundaries, filesystem/network behavior, timing behavior, or external integrations, attempt to construct focused cases with mocks, fakes, dependency injection, fixtures, or targeted harnesses.
    - Organize tests by module, submodule, and file structure so test locations mirror the production code structure.
    - Keep tests fast. Prefer deterministic unit tests, small fixtures, shared setup helpers, parallelizable cases, and narrow integration tests over slow broad scenarios unless broad coverage is required.
    - If a coverage target cannot be met because of tooling, environment, legacy structure, or infeasible isolation, state the blocker clearly and cover the highest-risk paths with the best available tests.

15. **Mandatory post-change review**
    - After writing or modifying code, perform a focused self-review before finalizing.
    - Review correctness, readability, responsibility boundaries, naming, constant usage, error handling, ownership/lifetime safety, coupling, testability, and consistency with these standards.
    - Review performance explicitly. Avoid extra allocations, copies, repeated scans, worse asymptotic complexity, unnecessary virtual dispatch, or heavier synchronization on hot paths unless there is a clear reason.
    - Small performance regressions are acceptable only when they buy meaningful correctness, maintainability, safety, or extensibility benefits; document the reason and expected impact.
    - Do not accept large or unbounded performance regressions. If the new implementation is much slower, meaningfully less scalable, or lower quality than the previous code, keep optimizing before presenting the work as complete.
    - When a performance tradeoff is intentional, explain why it is acceptable and prefer measuring or adding targeted tests/benchmarks when practical.

16. **Static analysis and Qodana discipline**
    - Treat Qodana, clang-tidy, and IDE inspections as triage inputs, not a single raw defect count.
    - Before reporting totals, record the exact profile, linter version, build directory or compilation database, header/source filters, exclusions, and whether numbers are raw diagnostics or deduplicated file-line-inspection findings.
    - Do not compare Qodana `qodana.starter` output directly with CLion `Project Default` or a custom clang-tidy command. Profile mismatch is a primary cause of apparently huge issue counts.
    - When a SARIF report says `configProfile: starter`, split real warnings from notes/hints before prioritizing work. A large count often contains style and modernization hints rather than production defects.
    - Qodana's `ClangTidy` rule can aggregate many different clang-tidy checks under one rule id. Expand it by message text or subcheck before deciding what kind of problem dominates.
    - Treat `CppRedundantQualifier` as a policy-sensitive inspection. If the project standard requires explicit `this->` or deliberate qualification for readability, classify these findings as style-policy conflict unless the qualifier is objectively wrong.
    - Detect stale reports before acting: if SARIF points at code that has already changed or a line no longer contains the reported pattern, mark the report stale and rerun analysis before counting it as remaining work.
    - Keep generated files, build directories, coverage output, cache directories, vendored code, Qodana output, and transient test artifacts out of analysis unless the user explicitly asks to inspect them.
    - Categorize findings by root cause: real production correctness/safety issue, real performance/ownership issue, style-only modernization, test/helper-only issue, generated/build artifact, stale compile database/config problem, or justified false positive.
    - Prioritize fixes in this order: correctness and lifetime bugs, use-after-move and branch-clone hazards, avoidable copies/moves on production hot paths, API const-correctness and ownership clarity, then readability/modernize cleanup.
    - For pass-by-value findings, distinguish cheap scalar handles from non-trivial payloads. Fix copied strings, vectors, AST/IR records, symbols, paths, maps, diagnostics, and type data when the callee only observes them; leave intentional sink APIs and cheap handles by value.
    - For C++ compiler projects, pay special attention to parser/AST/sema/IR/driver hot paths, compact-node materialization, sink APIs, arena/interner lifetime, and test helper wrappers that can hide new avoidable copies after core APIs are fixed.
    - When a finding is intentionally ignored, document the reason in the review result or local suppression. Do not hide warnings silently or broaden suppressions beyond the smallest stable scope.

## Working Guidance

- Follow existing project conventions unless they conflict with these standards; when they conflict, prefer these standards and keep the change scoped.
- When reviewing code, call out violations of these standards as maintainability issues.
- When editing existing code, avoid broad churn unless needed for the requested change, but do apply these standards to newly written or substantially modified C++ code.
- Do not finish a C++ coding task without running relevant tests and coverage checks, unless a concrete blocker prevents it.
- Do not finish a C++ coding task immediately after edits; run the mandatory post-change review and improve the code when the review finds quality or performance problems.

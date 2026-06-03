---
name: clean-code-refactor
description: Use when refactoring or implementing code where maintainability matters, especially when the user asks to remove magic numbers, avoid huge files/classes/functions, split responsibilities, preserve behavior, prepare code for LSP/incremental/concurrent workflows, or apply clean architecture/code-quality guardrails during compiler, parser, frontend, backend, or general code changes.
---

# Clean Code Refactor

## Core Rules

- Preserve behavior unless the user explicitly asks for a semantic change.
- Read the surrounding code first and follow local naming, ownership, and layering conventions.
- Keep edits scoped to the requested area. Do not rewrite unrelated files.
- Split by responsibility, not by arbitrary line count.
- Prefer small cohesive classes/modules over one coordinator class that owns every detail.
- Remove magic numbers and magic strings from logic unless they are obvious domain syntax tokens or idiomatic `0`, `1`, `-1`.
- Name constants after the domain concept, not the literal value.
- In C++ class methods, qualify member function calls and member field access with `this->`.
- Keep validation proportional: build and test the touched surface; run formatting or diff checks when available.

## Magic Numbers

When a numeric literal appears in executable logic, ask what concept it represents.

Use a named constant when the number is:

- A protocol, format, token, radix, width, limit, size, threshold, retry count, timeout, precedence, arity, alignment, or encoding detail.
- Repeated.
- Needed to understand parser, lexer, compiler, wire format, or storage behavior.
- Likely to change independently from the surrounding algorithm.

Prefer:

```cpp
constexpr int kHexRadix = 16;
constexpr base::usize kRadixPrefixLength = 2;
```

Instead of:

```cpp
if (text.size() > 2) {
    base = 16;
}
```

Do not over-abstract harmless loop/index idioms when the meaning is already structural and universal, such as empty checks, first element checks, boolean-to-index conversions, or sentinel comparisons already represented by domain types.

## File And Class Size

- Treat a file growing beyond roughly a few hundred lines as a design smell, not an automatic failure.
- Before adding to a large file, identify the smallest cohesive responsibility that can become its own helper/class/source file.
- Keep public headers concise. Move implementation details to `.cpp` files or local helpers.
- Avoid one class accumulating parsing, diagnostics, state, recovery, AST construction, and feature-specific syntax at once.
- Use coordinator classes sparingly: they may own shared session/cursor/diagnostic state, but feature logic should live in focused parts.

## C++ Member Access Style

- Inside non-static C++ member functions, write `this->member_` for member fields.
- Inside non-static C++ member functions, write `this->helper()` for member functions, including inherited protected helpers.
- Use `this->` for parser-part shared helpers such as `check`, `match`, `expect`, `parse_expr`, `parse_type`, `merge`, `report_here`, and `reset_panic`.
- Use `this->` for parser session members such as `session_` and bridge members such as `parser_`.
- Do not add `this->` to local variables, namespace functions, static functions, free functions, constructors in initializer lists, or enum/class names.
- Keep generated or third-party code unchanged unless the task explicitly includes it.

## Responsibility Split

For parser/compiler code, prefer this style of split:

- Session/cursor/diagnostics coordination.
- Top-level declarations/items.
- Types.
- Statements.
- Blocks.
- Control flow.
- Expressions by layer: precedence, postfix, primary, builtin.
- Patterns.
- Recovery/lookahead helpers.

Keep cross-part calls explicit and narrow. If one part starts needing many private details from another, reconsider the boundary.

## Refactor Workflow

1. Inspect file sizes, class declarations, and call graph before editing.
2. Identify concrete smells: magic literals, mixed responsibilities, duplicated logic, long functions, hidden state.
3. Make one behavior-preserving structural change at a time.
4. Replace magic literals with named constants or small helper functions near the domain logic.
5. Update build files when source files are split.
6. Run the relevant build/tests and a whitespace/diff check.
7. Report what changed, what passed, and what was intentionally left untouched.

## Review Checklist

- Are all nontrivial literals named?
- Do constants explain domain meaning rather than restating the value?
- Can a new contributor find the code for a feature without scanning a giant class?
- Are classes cohesive enough to describe in one sentence?
- Did the refactor avoid semantic changes?
- Did tests/builds cover the touched paths?
- Do C++ member accesses inside class methods use `this->` consistently?

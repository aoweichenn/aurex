# Aurex Parser Architecture

This document records the current parser split and the rules for extending it.
It is intentionally focused on parser internals, not on language syntax.

## Responsibility Map

The parser is split by grammar responsibility. New syntax should be placed in
the narrowest parser part that owns the relevant grammar surface.

| Component | Responsibility |
| --- | --- |
| `Parser` | Parse session coordination, token cursor wrappers, diagnostics, recovery entry point, source range helpers, and top-level module flow. |
| `ParserPartBase` | Shared bridge for parser parts that need cursor, diagnostics, AST storage, and cross-part entry points. |
| `RecoveryContext` helpers | Contextual recovery boundary sets for items, statements, expression statements, and future narrower parse regions. |
| `ItemParser` | Module paths, imports, visibility, top-level item dispatch, const/type aliases, generic parameter lists. |
| `TypeParser` | Type syntax, primitive types, named/scoped types, pointer types, array types, and type argument lists. |
| `StmtParser` | Statement dispatch, local declarations, expression statements, and assignment statements. |
| `BlockParser` | Statement blocks and block expressions. |
| `ControlStmtParser` | Control statements and their clauses: `if`, `for`, `while`, `break`, `continue`, `defer`, and `return`. |
| `ExprParser` | High-level expression dispatch, conditional/match expressions, unary operators, and table-driven binary precedence parsing. |
| `PrimaryExprParser` | Primary expression dispatch, literals, grouped expressions, block expressions, and builtin keyword dispatch. |
| `NameExprParser` | Identifier expressions, scoped names, and struct literals. |
| `PostfixExprParser` | Postfix expression suffixes: type arguments, field access, indexing, calls, and `?`. |
| `BuiltinExprParser` | Builtin expressions such as casts, pointer/address operations, move, and string helpers. |
| `PatternParser` | Match patterns. |

## Recovery Contexts

Parser recovery is intentionally contextual. `Parser::synchronize` accepts a
`RecoveryContext` so the caller can choose the nearest useful boundary instead
of always scanning for every possible grammar starter.

Current contexts:

- `RecoveryContext::item` is used by top-level parsing and item containers
  such as `impl` and `extern c` blocks. It stops at item starters and `}` so a
  malformed member does not consume the next declaration or the container end.
- `RecoveryContext::statement` is used inside statement blocks and block
  expressions. It stops at statement starters, expression-statement starters,
  semicolons, and `}`.
- `RecoveryContext::type_argument` is used inside `<...>` type argument lists.
  It stops at list separators, list closers, enclosing delimiters, and obvious
  outer grammar starters so malformed generic arguments do not cascade into the
  next declaration or statement.
- `RecoveryContext::match_arm` is used between `match` arms. It stops at arm
  separators, the closing `}`, valid pattern starters, and obvious outer grammar
  starters.
- `RecoveryContext::item_or_statement` remains the conservative default for
  bridge calls while a caller is being migrated to a narrower context.

When adding recovery behavior, name the recovery set after the grammar boundary
it represents. Do not inline a long token switch into a grammar parser part.

## Non-Negotiable Rules

- Keep `Parser` as a coordinator. Do not move feature-specific grammar logic
  back into it.
- Split by responsibility, not by arbitrary line count.
- Keep public headers concise. Prefer implementation-local helpers in `.cpp`
  files when a helper is not part of the parser part interface.
- Avoid magic numbers and magic strings in parser logic. Name domain values
  such as token arity, radix, delimiter width, binding power, and limits.
- Inside C++ class methods, qualify member function calls and member field
  access with `this->`.
- Use `ParserPartBase` range helpers such as `expr_range_or` and
  `stmt_range_or` when deriving source ranges from AST ids on recovery-prone
  paths. Do not index AST vectors directly just to read a node range.
- Preserve behavior during architecture refactors unless the task explicitly
  asks for syntax or semantic changes.
- Add or adjust focused parser tests before changing parsing algorithms,
  especially around ambiguous syntax.
- Update CMake whenever parser source files are split or added.

## Ambiguous Syntax Guardrails

Angle-list lookahead is intentionally isolated because `<...>` overlaps with
comparison operators, generic type arguments, scoped constructors, and struct
literals.

Protected cases:

- Nested generic type arguments that lex as `>>`.
- Shift expressions such as `a >> b`.
- Qualified generic enum constructors such as `Result<T, E>.ok(...)`.
- Generic struct literals such as `Wrap<Wrap<i32>> { ... }`.
- Conditions where struct literals must not be parsed as the condition value.

Before changing lookahead or expression parsing, add a focused parser regression
test for the ambiguous case. Binary operator changes should also preserve
left-associative AST shape unless the language design explicitly changes it.

## Expression Parsing

`ExprParser` owns only the high-level expression dispatcher and the infix
operator parser. Primary, postfix, and builtin expression details remain in
their focused parser parts.

Binary operators are parsed by a precedence table plus precedence climbing. New
binary operators should be added to the table with an explicit precedence entry
instead of adding another recursive-descent layer. This keeps operator syntax
auditable and avoids growing one function per precedence level.

## Current Refactor Boundary

The current parser is still a recursive descent parser. The existing split is
a maintainability boundary, not the final parsing algorithm.

Near-term improvements:

- Keep deduplicating small lookahead and recovery helpers.
- Add narrower recovery contexts where the grammar has a real boundary, such as
  call argument lists and struct literal fields.
- Keep tests close to grammar boundaries that are likely to regress.
- Make parser parts easier to reason about before algorithmic changes.

Medium-term expression parser improvement:

- Evaluate whether prefix/postfix/infix expression parsing should converge into
  a Pratt-style parser after the current primary/postfix split is stable enough.

Longer-term LSP/incremental parsing direction:

- Separate token stream navigation from AST construction more strongly.
- Introduce parser events or a recoverable syntax tree layer before committing
  to a green-tree representation.
- Keep diagnostics recoverable and localized so partial parses remain useful.

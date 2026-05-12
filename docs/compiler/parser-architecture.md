# Aurex Parser Architecture

This document records the current parser split and the rules for extending it.
It is intentionally focused on parser internals, not on language syntax.

## Responsibility Map

The parser is split by grammar responsibility. New syntax should be placed in
the narrowest parser part that owns the relevant grammar surface.

| Component | Responsibility |
| --- | --- |
| `Parser` | Parse session coordination, token cursor wrappers, diagnostics, recovery entry point, source range helpers, and top-level module flow. |
| `ParserPartCore` | Shared cursor, diagnostics, recovered `expect`, and parse-session access for grammar parser parts. |
| `ParserPartRouter` | Cross-part parse entry points such as `parse_type`, `parse_expr`, `parse_stmt`, and `parse_pattern`. |
| `ParserPartRangeReader` | AST-id-to-source-range helpers used on recovery-prone paths. |
| `ParserPartBase` | Thin compatibility base that composes the core/router/range-reader layers for concrete parser parts. |
| `RecoveryContext` helpers | Contextual recovery boundary sets for items, statements, expression statements, and future narrower parse regions. |
| `ItemParser` | Module paths, imports, visibility, top-level item dispatch, const/type aliases, generic parameter lists. |
| `TypeParser` | Type syntax, primitive types, named/scoped types, pointer types, array types, and type argument lists. |
| `StmtParser` | Statement dispatch, local declarations, expression statements, and assignment statements. |
| `BlockParser` | Statement blocks and block expressions. |
| `ControlStmtParser` | Control statements and their clauses: `if`, `for`, `while`, `break`, `continue`, `defer`, and `return`. |
| `ExprParser` | High-level expression dispatch, conditional/match expressions, unary operators, and table-driven binary precedence parsing. |
| `PrimaryExprParser` | Primary expression dispatch, literals, grouped expressions, block expressions, and builtin keyword dispatch. |
| `NameExprParser` | Identifier expressions, scoped names, and struct literals. |
| `PostfixExprParser` | Postfix expression suffixes: explicit generic apply, field access, indexing, calls, and `?`. |
| `BuiltinExprParser` | Builtin expressions such as casts, pointer/address operations, and string helpers. |
| `PatternParser` | Match patterns. |

Parser part declarations are split by responsibility under
`include/aurex/parse/*_part.hpp`. Source files should include the narrowest
part header that declares the class they implement, plus any concrete parser
part they instantiate directly. `parser_parts.hpp` is only a compatibility
umbrella for callers that intentionally need every parser part declaration; do
not add new parser class declarations directly to it.

The shared parser-part bridge is layered deliberately:

- `ParserPartCore` owns token navigation, diagnostics, and recovered `expect`
  helpers.
- `ParserPartRouter` owns cross-parser calls and is the only shared layer that
  instantiates another concrete parser part.
- `ParserPartRangeReader` owns AST range lookup helpers and keeps recovery
  source-range fallback logic out of grammar parser parts.
- `ParserPartBase` is intentionally thin. Do not add new helper families
  directly to it; add them to the narrow layer that owns the responsibility.

`Parser` implementation is also split by infrastructure responsibility:

- `parser.cpp` owns construction and top-level module flow only.
- `parser_cursor.cpp` owns cursor forwarding used by grammar parser parts.
- `parser_diagnostics.cpp` owns `expect`, recovered `expect`, synchronization,
  and diagnostic forwarding.
- `parser_source_ranges.cpp` owns source-range composition.
- `[]` generic lookahead is local to the parser part that owns the ambiguous
  grammar surface. Struct literal type-argument lookahead stays in
  `parser_name_expr.cpp`; explicit generic function apply is handled in
  `parser_postfix.cpp`; named type arguments are handled in `parser_type.cpp`.

Recovery token sets are split into source-private start-token, list-boundary,
and delimiter-boundary files. The public recovery API should stay limited to
context selection and the small starter predicates that grammar parser parts
already need.

## Recovery Contexts

Parser recovery is intentionally contextual. `Parser::synchronize` accepts a
`RecoveryContext` so the caller can choose the nearest useful boundary instead
of always scanning for every possible grammar starter.

Current contexts:

- `RecoveryContext::identifier` is used at required declaration, member,
  scoped-name, and pattern name positions. It stops at the next identifier,
  common punctuation boundaries, item starters, and obvious statement starters.
- `RecoveryContext::item` is used by top-level parsing and item containers
  such as `impl` and `extern c` blocks. It stops at item starters and `}` so a
  malformed member does not consume the next declaration or the container end.
- `RecoveryContext::statement` is used inside statement blocks and block
  expressions. It stops at statement starters, expression-statement starters,
  semicolons, and `}`.
- `RecoveryContext::generic_type_argument` is used inside `[...]` type argument lists.
  It stops at list separators, list closers, enclosing delimiters, and obvious
  outer grammar starters so malformed generic arguments do not cascade into the
  next declaration or statement.
- `RecoveryContext::match_arm` is used between `match` arms. It stops at arm
  separators, the closing `}`, valid pattern starters, and obvious outer grammar
  starters.
- `RecoveryContext::call_argument` is used inside function and method call
  argument lists. It stops at argument separators, call closers, enclosing
  delimiters, and obvious outer grammar starters.
- `RecoveryContext::struct_field` is used inside struct literal field lists. It
  stops at field separators, the closing `}`, valid field starters, and obvious
  outer grammar starters.
- `RecoveryContext::parameter` is used inside function parameter lists. It
  stops at parameter separators, `)`, return/body/prototype boundaries, valid
  parameter starters, and obvious outer grammar starters.
- `RecoveryContext::struct_decl_field` is used inside struct declaration field
  lists. It stops at field separators, `}`, declaration starters, and obvious
  outer grammar starters.
- `RecoveryContext::enum_case` is used inside enum case lists. It stops at case
  separators, `}`, declaration starters, and obvious outer grammar starters.
- `RecoveryContext::generic_parameter` is used inside generic parameter lists.
  It stops at parameter separators, `]`, declaration followers, valid generic
  parameter starters, and obvious outer grammar starters.
- `RecoveryContext::parameter_list_start` is used before function parameter
  lists. It stops at `(`, legal parameter starters, signature followers, and
  obvious outer grammar starters so an inserted token before `(` does not
  damage the whole signature.
- `RecoveryContext::abi_attribute_argument` is used inside ABI attribute
  argument parentheses. It stops at the argument value, `)`, function
  body/prototype boundaries, and obvious outer grammar starters.
- `RecoveryContext::abi_attribute_start` is used before ABI attribute argument
  parentheses. It stops at `(`, a direct string value for missing-paren
  recovery, signature/body boundaries, and obvious outer grammar starters.
- `RecoveryContext::builtin_argument` is used inside builtin expression
  argument lists after a malformed separator. It stops at separators, `)`,
  enclosing delimiters, valid expression starters, and obvious outer grammar
  starters so fixed-shape builtins can still parse their next argument.
- `RecoveryContext::builtin_argument_list_start` is used before builtin
  argument lists. It stops at `(`, valid first argument starters, enclosing
  delimiters, and obvious outer grammar starters so both inserted-token and
  missing-paren cases stay local.
- `RecoveryContext::grouped_expression` is used after a malformed
  parenthesized expression. It stops at `)`, common enclosing delimiters, and
  obvious outer grammar starters.
- `RecoveryContext::index_expression` is used after a malformed indexing
  expression. It stops at `]`, common enclosing delimiters, and obvious outer
  grammar starters.
- `RecoveryContext::array_type_length` is used after a malformed array type
  length. It stops at `]`, common enclosing delimiters, and obvious outer
  grammar starters so the element type can still be parsed.
- `RecoveryContext::pattern_payload` is used after a malformed enum pattern
  payload binding. It stops at `)`, match-arm separators, `=>`, and obvious
  outer grammar starters.
- `RecoveryContext::enum_case_payload` is used after a malformed enum
  declaration payload type. It stops at `)`, `=`, case separators, declaration
  ends, and obvious outer grammar starters.
- `RecoveryContext::path_segment` is used inside module and import paths. It
  stops at path separators, path terminators, import aliases, valid path
  segment starters, and obvious outer grammar starters.
- `RecoveryContext::import_alias` is used after a malformed `import ... as`
  alias. It stops at the import terminator or obvious outer grammar starters so
  the import declaration can close locally.
- `RecoveryContext::module_terminator` is used after a malformed top-level
  `module` declaration path. It stops at `;`, item/import starters, and obvious
  statement starters so imports and declarations remain reachable.
- `RecoveryContext::item_terminator` is used after malformed item-level
  declarations that must end in `;`. It stops at `;`, container ends, item
  starters, and obvious statement starters.
- `RecoveryContext::type_annotation` is used before required type annotations
  and enum base types. It stops at `:`, valid type starters, common declaration
  boundaries, and obvious outer grammar starters.
- `RecoveryContext::initializer` is used before required initializer/value
  separators such as declaration `=` and enum case `=`. It stops at `=`, valid
  expression or type starters, common declaration boundaries, and obvious outer
  grammar starters.
- `RecoveryContext::match_arm_arrow` is used between a match pattern/guard and
  its arm value. It stops at `=>`, expression starters, arm separators, the
  match closer, and obvious outer grammar starters.
- `RecoveryContext::if_else` is used by if expressions before the required
  `else` branch. It stops at `else`, block starts, enclosing delimiters, and
  obvious outer grammar starters.
- `RecoveryContext::statement_terminator` is used after malformed
  control-statement tails and block-expression assignment tails. It stops at
  semicolons, block ends, item starters, and non-expression statement starters.
- `RecoveryContext::for_clause_separator` is used after a malformed `for`
  condition clause. It stops at the second `;`, a loop body `{`, block ends,
  item starters, and non-expression statement starters.
- `RecoveryContext::block_start` is used before parsing a block body. It stops
  at `{`, `}`, item starters, and statement starters so a malformed control
  header can still attach to the intended body.
- `RecoveryContext::block_end` is used when a block tail is malformed or
  missing. It stops at `}` or the next item starter so one missing brace does
  not consume following declarations.
When adding recovery behavior, name the recovery set after the grammar boundary
it represents. Do not inline a long token switch into a grammar parser part.

## Non-Negotiable Rules

- Keep `Parser` as a coordinator. Do not move feature-specific grammar logic
  back into it.
- Keep `Parser` infrastructure implementation in the focused source file that
  owns the concern: cursor, diagnostics/recovery, source ranges, or lookahead.
- Split by responsibility, not by arbitrary line count.
- Keep public headers concise. Prefer implementation-local helpers in `.cpp`
  files when a helper is not part of the parser part interface.
- Keep parser part declarations in their focused `*_part.hpp` header. Do not
  grow `parser_parts.hpp` back into a declaration dump.
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

Generic `[]` lookahead is intentionally isolated because the same delimiter is
also used by array types, index expressions, and builtin type arguments.

Protected cases:

- Array types such as `[4]u8`.
- Array literals and repeat literals such as `[1, 2, 3]` and `[0; 128]`.
- Index expressions such as `items[index]`.
- Builtin type arguments such as `cast[i32](value)` and `sizeof[T]`.
- Explicit generic function calls such as `id::[i32](value)`. The `::[...]`
  suffix is represented as a separate `generic_apply` expression whose callee is
  the function name expression.
- Generic struct literals such as `Wrap[Wrap[i32]] { ... }`.
- Conditions where struct literals must not be parsed as the condition value.
- Legacy `<...>` syntax is not a recovery target for generics. `<` and `>` are
  comparison or shift-related tokens in the language grammar.

Generic parameter lists and type argument lists are non-empty. `fn f[]`,
`Box[]`, and `id::[](...)` are parser errors instead of recoverable shorthand
for inference.

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
- Add narrower recovery contexts where the grammar has a real boundary instead
  of introducing broad default synchronization behavior.
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

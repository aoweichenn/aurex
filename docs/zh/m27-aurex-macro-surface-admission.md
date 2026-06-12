# Aurex M27 Aurex Macro Surface Admission

阶段：M27 Aurex Macro Surface Admission

## 阶段定位

M27 是 Aurex 宏系统从“只围绕 item attribute / builtin derive 做内部边界”进入“用户可声明宏入口”的第一步。
它不是完整宏展开实现，也不是 Rust `macro_rules!` 的移植。本阶段只把 Aurex 自己风格的宏声明表面落到
parser / AST / debug dump / query facts / early expansion admission gate，供后续 matcher、hygiene、derive lowering
和编译期执行沙箱继续接入。

M27 保持现有 M21-M26 builtin derive 链条兼容：`EarlyItemExpansionResult::name` 仍固定为
`M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure`，默认
`expand_early_item_macros_noop()` 仍接受 `m21c_macro_expansion_plan_baseline()`。M27 增量新增的是
`AurexMacroSurfaceAdmissionGate` 和 `m27_macro_expansion_plan_baseline()`，不替换 M21-M26 结果格式。

## 为什么不采用 Rust macro_rules

Aurex 当前明确不采用 `macro_rules!` / `$($x:expr),*` 这类表面，原因不是“Rust 做不到”，而是不符合本项目当前目标：

- Aurex 不希望把用户宏入口设计成标点密集的 token pattern DSL；这类写法对错误定位、编辑器结构化索引和后续 typed
  matcher 都不友好。
- Aurex 已经把宏地基设计为 query-backed incremental expansion，需要稳定 query key、body fingerprint、debug dump
  和 admission gate，而不是优先追求文本替换能力。
- Aurex 当前阶段仍不实现标准库、runtime helper、external process、用户编译期代码执行或 generated code merge。
  如果照搬 proc-macro / macro_rules 表面，会让用户误以为这些能力已经可用。
- Aurex 希望 matcher 后续可以向 typed matcher、item matcher、derive target matcher 和 token matcher 演化，而不是把
  `$` fragment 语法变成长期包袱。

因此 M27 采用上下文关键字 `macro`，不加入 lexer keyword 表，也不支持 `macro_rules!`。

## 当前语法

M27 当前接受三类 item 位置的宏声明：

```aurex
macro VecBuilder {
  match expr_list(xs) -> { xs }
}

macro derive Inspect {
  match item(target) -> { target }
}

macro const TokenBuild {
  match tokens(input) -> { input }
}
```

语法骨架：

```text
MacroDecl       = Visibility? "macro" MacroFlavor? Identifier MacroBody
MacroFlavor     = "derive" | "const"
MacroBody       = "{" TokenTree* "}"
MacroMatchHint  = "match" TokenTree* "->" TokenTree
```

含义：

- `macro Name { ... }`：声明式宏表面，当前记录为 `MacroDeclKind::declarative`。
- `macro derive Name { ... }`：用户自定义 derive 表面，当前记录为 `MacroDeclKind::derive`。
- `macro const Name { ... }`：编译期执行宏表面，当前记录为 `MacroDeclKind::compile_time`。
- 宏体当前只保存 flat token tree。`match ... -> ...` 是 admission/debug hint，不是已经执行的 matcher DSL。
- `macro` 和 `derive` 是上下文标记；`macro` 只有在 item 起始位置触发宏声明解析。
- `const` 在 `macro const Name` 中复用现有 `const` token。
- 宏声明可以带普通 item visibility，例如 `pub macro Name { ... }`。

## 当前能做什么

Parser / AST 层：

- 解析 `macro Name { ... }`、`macro derive Name { ... }`、`macro const Name { ... }`。
- 在 AST 中记录 `ItemKind::macro_decl`。
- 记录 `MacroDeclKind::declarative`、`MacroDeclKind::derive`、`MacroDeclKind::compile_time`。
- 保存宏体 `macro_body_tokens`、`macro_body_range`、`macro_match_clause_count` 和 `macro_body_balanced`。
- AST dump 会输出 `macro_kind=...`、`body_tokens=...`、`match_clauses=...`、`balanced=yes/no` 和 `macro_body`。
- tooling session 能把该 item kind 识别为 `macro`；IDE symbol metadata 当前不把它误投影成 value/type definition。

Early expansion 层：

- `expand_early_item_macros_noop()` 会扫描源 AST 中的 `ItemKind::macro_decl`。
- 每个宏声明生成一个 `AurexMacroSurfaceAdmissionGate`。
- gate 记录 source item 数、body fingerprint、admission identity、query name、宏 kind、宏名、body token count、match
  clause count、body balance 和 blocker reason。
- summary / dump / fingerprint 会包含：
  - `aurex_macro_surface_source_items`
  - `aurex_macro_surface_admissions`
  - `aurex_macro_declarative_surfaces`
  - `aurex_macro_user_derive_surfaces`
  - `aurex_macro_compile_time_surfaces`
  - `aurex_macro_surface_match_clauses`
  - `aurex_macro_surface_expansion_enabled`
  - `aurex_macro_surface_compile_time_execution_enabled`
  - `aurex_macro_surface_parser_consumable`
- validation 会拒绝 source item / gate 数不匹配、body 不平衡、query name 漂移、admission identity 漂移、错误 surface
  kind、打开 expansion / compile-time execution / parser consumption / AST mutation / sema-visible generated items /
  standard library / runtime / external process / user generated code。

Query 层：

- 新增 `MacroExpansionFactKind::aurex_declarative_macro_surface`。
- 新增 `MacroExpansionFactKind::aurex_user_derive_macro_surface`。
- 新增 `MacroExpansionFactKind::aurex_compile_time_macro_execution_admission`。
- 新增对应 policy：
  - `aurex_declarative_macro_surface_v1`
  - `aurex_user_derive_macro_surface_v1`
  - `aurex_compile_time_macro_execution_admission_v1`
- 新增 `m27_macro_expansion_plan_baseline()` 和 `is_valid_m27_macro_expansion_plan()`。
- M27 plan 是 M21c 7 个 facts 加 3 个 Aurex macro surface facts，共 10 个 facts。

## 当前不能做什么

M27 明确不实现以下能力：

- 不支持 Rust `macro_rules!`。
- 不支持 `$matcher` / `$($x:expr),*` 语法。
- 不真实展开宏。
- 不执行用户编译期代码。
- 不把宏输出插回 parser。
- 不生成 generated source text。
- 不 parse / merge generated module part。
- 不修改 AST。
- 不产生 sema-visible generated items。
- 不生成用户代码。
- 不实现用户自定义 derive lowering。
- 不实现 expression macro / statement macro。
- 不实现真实 hygiene resolution。
- 不实现 debug trace CLI 或 `--emit-expanded`。
- 不要求或引入标准库。
- 不要求 runtime helper。
- 不启动 external process。

## 设计不变量

M27 的核心不变量：

- 源里出现的 `macro_decl` 数量必须等于 `aurex_macro_surface_admission_gates` 数量。
- 每个 gate 必须且只能属于一种 surface：declarative、user derive 或 compile-time execution admission。
- `admission_identity` 必须由 item/module/part/attached part、macro kind、macro name、body fingerprint、body token
  count、match clause count、body balance 和 query name 稳定推导。
- `body_fingerprint` 必须非零，且随着宏体 token tree 漂移。
- gate 必须保持 `expansion_enabled=false`、`compile_time_execution_enabled=false`、
  `parser_consumption_enabled=false`、`ast_mutated=false`、`sema_visible_generated_items=false`。
- gate 必须保持 `standard_library_required=false`、`runtime_required=false`、`external_process_required=false`、
  `produced_user_generated_code=false`。

这些不变量让 M27 成为后续真实宏能力的 admission boundary，而不是半隐式展开实现。

## 下一步

建议 M27 后续按下面顺序推进：

1. Typed matcher model：把当前 `match ... -> ...` 从 debug hint 升级为结构化 matcher AST，优先支持 expr list、item
   target、token stream 三类，不引入 `$` 语法。
2. Hygiene / name scopes：把 M21f 的 hygiene stub 从 macro input 扩展到 macro declaration definition site，并设计
   declaration-local fresh name。
3. Dry-run parser admission：允许 compiler-owned generated token stream 做受控 parse dry-run，但仍保持 rollback、diagnostic
   shadow 和 no-AST-mutation 闭包。
4. User derive lowering design：为 `macro derive Name` 设计 derive target schema、capability/output 声明和错误定位。
5. Compile-time execution sandbox：在 const eval / comptime 子集、资源限制、权限模型、deterministic fingerprint 和 diagnostic
   replay 设计完成前，不执行 `macro const`。

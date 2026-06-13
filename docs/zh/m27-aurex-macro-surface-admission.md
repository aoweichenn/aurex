# Aurex M27 Aurex Macro Surface Admission

阶段：M27 Aurex Macro Surface Admission

附加收口：M27b Aurex Typed Matcher And Definition-Site Hygiene Admission

## 阶段定位

M27 是 Aurex 宏系统从“只围绕 item attribute / builtin derive 做内部边界”进入“用户可声明宏入口”的第一步。
它不是完整宏展开实现，也不是 Rust `macro_rules!` 的移植。本阶段只把 Aurex 自己风格的宏声明表面落到
parser / AST / debug dump / query facts / early expansion admission gate，供后续 matcher、hygiene、derive lowering
和编译期执行沙箱继续接入。

M27 保持现有 M21-M26 builtin derive 链条兼容：`EarlyItemExpansionResult::name` 仍固定为
`M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure`，默认
`expand_early_item_macros_noop()` 仍接受 `m21c_macro_expansion_plan_baseline()`。M27 增量新增的是
`AurexMacroSurfaceAdmissionGate` 和 `m27_macro_expansion_plan_baseline()`，不替换 M21-M26 结果格式。

M27b 在 M27 表面之上继续前进一小步：不执行宏、不把输出交给 parser、不修改 AST，而是把宏体中 Aurex 自己风格的
顶层 `match head(binding) -> { ... }` 写法转成结构化 typed matcher admission facts，并为每个宏声明建立
definition-site hygiene admission gate。M27b 仍然是 admission-only，不是完整用户宏系统。

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

M27/M27b 当前接受三类 item 位置的宏声明，并在宏体中索引三类第一版 matcher head：

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
MacroMatcherAdmission = "match" MacroMatcherHead "(" Identifier ")" "->" "{" TokenTree* "}"
MacroMatcherHead = "expr_list" | "item" | "tokens"
```

含义：

- `macro Name { ... }`：声明式宏表面，当前记录为 `MacroDeclKind::declarative`。
- `macro derive Name { ... }`：用户自定义 derive 表面，当前记录为 `MacroDeclKind::derive`。
- `macro const Name { ... }`：编译期执行宏表面，当前记录为 `MacroDeclKind::compile_time`。
- 宏体当前保存为 flat token tree。M27b 会在 early expansion no-op 边界识别顶层
  `match expr_list(xs) -> { xs }`、`match item(target) -> { target }` 和
  `match tokens(input) -> { input }`，但它们仍只是 admission facts，不会真正匹配调用点或执行输出。
- `macro` 和 `derive` 是上下文标记；`macro` 只有在 item 起始位置触发宏声明解析。
- `const` 在 `macro const Name` 中复用现有 `const` token。
- 宏声明可以带普通 item visibility，例如 `pub macro Name { ... }`。
- 未知 matcher head 例如 `match unknown(input) -> { input }` 会记录为 unknown matcher admission gate，并保持 blocked；
  当前不会把它解释成错误恢复后的真实 matcher。

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
- M27b 每个宏声明还生成一个 `AurexMacroDefinitionSiteHygieneAdmissionGate`。
- M27b 每个顶层 `match` clause 生成一个 `AurexMacroTypedMatcherAdmissionGate`。
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
- M27b summary / dump / fingerprint 还会包含：
  - `aurex_macro_definition_site_hygiene_gates`
  - `aurex_macro_definition_site_scope_available`
  - `aurex_macro_fresh_name_scopes`
  - `aurex_macro_diagnostic_anchors`
  - `aurex_macro_hygiene_resolution_enabled`
  - `aurex_macro_typed_matcher_admissions`
  - `aurex_macro_typed_matchers_recognized`
  - `aurex_macro_expr_list_matchers`
  - `aurex_macro_item_matchers`
  - `aurex_macro_token_stream_matchers`
  - `aurex_macro_unknown_matchers`
  - `aurex_macro_typed_matcher_execution_enabled`
- validation 会拒绝 source item / gate 数不匹配、body 不平衡、query name 漂移、admission identity 漂移、错误 surface
  kind、打开 expansion / compile-time execution / parser consumption / AST mutation / sema-visible generated items /
  standard library / runtime / external process / user generated code。
- M27b validation 还会拒绝 surface gate 缺少 definition-site hygiene gate、顶层 `match` clause 缺少 typed matcher
  gate、hygiene identity 漂移、matcher identity 漂移、matcher kind flags 漂移、typed matcher execution 被打开、
  hygiene resolution 被打开、declared names 被 lookup/export/sema 看见、parser consumption 被打开、AST mutation
  被打开或 user generated code 被打开。

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
- M27b 新增 `MacroExpansionFactKind::aurex_macro_typed_matcher_admission`。
- M27b 新增 `MacroExpansionFactKind::aurex_macro_definition_site_hygiene_admission`。
- M27b 新增 `MacroExpansionFactKind::aurex_macro_debuggable_diagnostic_anchor`。
- M27b 新增对应 policy：
  - `aurex_macro_typed_matcher_admission_v1`
  - `aurex_macro_definition_site_hygiene_admission_v1`
  - `aurex_macro_debuggable_diagnostic_anchor_v1`
- M27b 新增 `m27b_macro_expansion_plan_baseline()` 和 `is_valid_m27b_macro_expansion_plan()`。
- M27b plan 是 M27 10 个 facts 加三类 typed matcher / hygiene / diagnostic anchor facts，共 13 个 facts。

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
- typed matcher execution is admission-only in M27b。
- definition-site hygiene resolution is admission-only in M27b。
- 仍不展开宏/不执行用户编译期代码/不消费 parser/不修改 AST。

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

M27b 的新增不变量：

- 每个 `AurexMacroSurfaceAdmissionGate` 必须有且只有一个对应的
  `AurexMacroDefinitionSiteHygieneAdmissionGate`。
- 每个顶层 `match` clause 必须有一个对应的 `AurexMacroTypedMatcherAdmissionGate`。
- typed matcher gate 必须绑定 surface admission identity、body fingerprint、definition-site hygiene identity、
  matcher fingerprint、diagnostic anchor identity 和 stable query name。
- `expr_list`、`item`、`tokens`、`unknown` 四类 matcher flag 必须且只能有一类为真。
- `expr_list` / `item` / `tokens` 会记录 `matcher_shape_recognized=true`；未知 head 会记录
  `matcher_shape_recognized=false` 并保持 blocked。
- definition-site hygiene gate 必须保留 `definition_site_scope_available=true`、
  `fresh_name_scope_reserved=true` 和 `diagnostic_anchor_available=true`，同时保持
  `hygiene_resolution_enabled=false`、`declared_names_visible=false` 和
  `captures_call_site_locals=false`。
- typed matcher gate 必须保持 `matcher_execution_enabled=false`、`expansion_enabled=false`、
  `compile_time_execution_enabled=false`、`parser_consumption_enabled=false`、`ast_mutated=false` 和
  `sema_visible_generated_items=false`。
- M27b gate 仍必须保持 `standard_library_required=false`、`runtime_required=false`、
  `external_process_required=false` 和 `produced_user_generated_code=false`。

## 下一步

M27b 已完成 typed matcher / definition-site hygiene admission。建议后续按下面顺序推进：

1. Matcher call-site admission：设计宏调用点 token capture、参数 shape fact 和调用点 diagnostic anchor，但仍不展开。
2. User derive lowering design：为 `macro derive Name` 设计 derive target schema、capability/output 声明和错误定位。
3. Dry-run parser admission：允许 compiler-owned generated token stream 做受控 parse dry-run，但仍保持 rollback、diagnostic
   shadow 和 no-AST-mutation 闭包。
4. Compile-time execution sandbox：在 const eval / comptime 子集、资源限制、权限模型、deterministic fingerprint 和 diagnostic
   replay 设计完成前，不执行 `macro const`。

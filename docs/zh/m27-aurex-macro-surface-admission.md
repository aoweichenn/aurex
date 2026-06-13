# Aurex M27 Aurex Macro Surface Admission

阶段：M27d Aurex Macro Output Contract Admission

历史收口：M27 Aurex Macro Surface Admission

附加收口：M27b Aurex Typed Matcher And Definition-Site Hygiene Admission

附加收口：M27c Aurex Macro Call-Site And User Derive Target Schema Admission

本轮收口：M27d Aurex Macro Output Contract Admission

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

M27c 在 M27/M27b 的基础上把“用户能写调用点”和“用户 derive 能看到目标 schema”两块 admission 边界落地：
parser / AST 现在能索引 item 位置的 `macro call Name { ... }`；early expansion 会为调用点生成 call-site
admission gate，并在同模块同名宏声明存在时建立 matcher-to-call binding admission gate；`#[derive(Name)]` 如果
匹配同模块 `macro derive Name { ... }`，会记录目标 `struct` / `enum` 的 schema admission gate。M27c 仍不展开、
不执行、不消费 parser、不修改 AST、不产生 sema-visible generated item。

M27d 在 M27c 的 matcher-to-call binding 和 user derive target schema 之后继续前进一步：不生成宏输出 token、
不把输出交给 parser、不修改 AST，而是为每个可进入后续输出路径的输入建立 compiler-owned macro output contract
admission。每个 contract 同时保留 future output token buffer identity、generated `ModulePartKey`、declared-name
policy admission gate 和 diagnostic projection admission gate。M27d 的重点是把“未来会产生什么输出、谁拥有、哪些名字
不可见、诊断如何投影”固定成 query-backed facts；它仍不是真实宏展开。

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

M27/M27b 当前接受三类 item 位置的宏声明，并在宏体中索引三类第一版 matcher head。M27c 额外接受 item
位置的 macro call-site：

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

macro call VecBuilder {
  1, nested(2, [3]), { value: 4 }
}

#[derive(Inspect)]
struct Config { threads: i32; enabled: bool; }
```

语法骨架：

```text
MacroDecl       = Visibility? "macro" MacroFlavor? Identifier MacroBody
MacroFlavor     = "derive" | "const"
MacroBody       = "{" TokenTree* "}"
MacroCall       = "macro" "call" Identifier MacroCallBody
MacroCallBody   = "{" TokenTree* "}"
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
- `macro call Name { ... }`：M27c 的 item-level 宏调用点表面。调用体保存为 flat token tree，并形成 call-site
  admission gate；它不是 expression macro 或 statement macro，也不会让宏输出进入 parser。
- `#[derive(Name)]` 与同模块 `macro derive Name { ... }` 匹配时，M27c 只记录目标 item 的 schema admission；
  当前不会执行用户 derive lowering，也不会生成 impl/item。
- `macro` 和 `derive` 是上下文标记；`macro` 只有在 item 起始位置触发宏声明解析。
- `call` 也是 `macro` 后的上下文标记；`macro call Name { ... }` 被记录为 `ItemKind::macro_call`，不记录为
  `MacroDeclKind`。
- `const` 在 `macro const Name` 中复用现有 `const` token。
- 宏声明可以带普通 item visibility，例如 `pub macro Name { ... }`。
- 未知 matcher head 例如 `match unknown(input) -> { input }` 会记录为 unknown matcher admission gate，并保持 blocked；
  当前不会把它解释成错误恢复后的真实 matcher。

## 当前能做什么

Parser / AST 层：

- 解析 `macro Name { ... }`、`macro derive Name { ... }`、`macro const Name { ... }`。
- 解析 `macro call Name { ... }`，并在 AST 中记录 `ItemKind::macro_call`。
- 在 AST 中记录 `ItemKind::macro_decl`。
- 记录 `MacroDeclKind::declarative`、`MacroDeclKind::derive`、`MacroDeclKind::compile_time`。
- 保存宏体 `macro_body_tokens`、`macro_body_range`、`macro_match_clause_count` 和 `macro_body_balanced`。
- 保存调用点 `macro_call_tokens`、`macro_call_range` 和 `macro_call_balanced`。
- AST dump 会输出 `macro_kind=...`、`body_tokens=...`、`match_clauses=...`、`balanced=yes/no` 和 `macro_body`。
- AST dump 会输出 `macro_call ... call_tokens=... balanced=yes/no` 和 `macro_call_body`。
- tooling session 能把该 item kind 识别为 `macro`；IDE symbol metadata 当前不把它误投影成 value/type definition。
- tooling session 能把 call-site item 识别为 `macro_call`；IDE symbol metadata 当前不把 macro call 误投影成 stable
  definition。

Early expansion 层：

- `expand_early_item_macros_noop()` 会扫描源 AST 中的 `ItemKind::macro_decl`。
- 每个宏声明生成一个 `AurexMacroSurfaceAdmissionGate`。
- M27b 每个宏声明还生成一个 `AurexMacroDefinitionSiteHygieneAdmissionGate`。
- M27b 每个顶层 `match` clause 生成一个 `AurexMacroTypedMatcherAdmissionGate`。
- M27c 每个 `ItemKind::macro_call` 生成一个 `AurexMacroCallSiteAdmissionGate`。
- M27c 对同模块同名且已声明的 macro call，生成一个 `AurexMacroMatcherToCallBindingAdmissionGate`，当前绑定第一个
  recognized typed matcher；缺失目标宏时只保留 call-site blocker，不生成 binding gate。
- M27c 对匹配同模块 `macro derive Name` 的 `#[derive(Name)]`，生成一个
  `AurexUserDeriveTargetSchemaAdmissionGate`，记录目标 kind、字段数、enum case 数和 enum payload 数。
- M27d 对每个 `AurexMacroMatcherToCallBindingAdmissionGate` 生成一个
  `AurexMacroOutputContractAdmissionGate`。
- M27d 对每个 `AurexUserDeriveTargetSchemaAdmissionGate` 生成一个
  `AurexMacroOutputContractAdmissionGate`。
- M27d 对每个 output contract 同步生成一个 `AurexMacroOutputDeclaredNamePolicyAdmissionGate` 和一个
  `AurexMacroOutputDiagnosticProjectionAdmissionGate`。
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
- M27c summary / dump / fingerprint 还会包含：
  - `aurex_macro_call_site_source_items`
  - `aurex_macro_call_site_admissions`
  - `aurex_macro_call_site_targets_declared`
  - `aurex_macro_call_site_balanced`
  - `aurex_macro_call_site_expansion_enabled`
  - `aurex_macro_matcher_to_call_bindings`
  - `aurex_macro_matcher_to_call_bindings_admitted`
  - `aurex_user_derive_target_schema_source_derives`
  - `aurex_user_derive_target_schemas`
  - `aurex_user_derive_target_schema_structs`
  - `aurex_user_derive_target_schema_enums`
  - `aurex_user_derive_target_schema_fields`
  - `aurex_user_derive_target_schema_enum_cases`
  - `aurex_user_derive_target_schema_enum_payloads`
- M27d summary / dump / fingerprint 还会包含：
  - `aurex_macro_output_contracts`
  - `aurex_macro_output_contract_call_bindings`
  - `aurex_macro_output_contract_user_derives`
  - `aurex_macro_output_contract_compiler_owned`
  - `aurex_macro_output_contract_source_maps`
  - `aurex_macro_output_contract_hygiene_marks`
  - `aurex_macro_output_contract_diagnostic_projections`
  - `aurex_macro_output_contract_declared_name_policies`
  - `aurex_macro_output_declared_name_policies`
  - `aurex_macro_output_declared_name_sets_reserved`
  - `aurex_macro_output_lookup_visible_declared_names`
  - `aurex_macro_output_diagnostic_projections`
  - `aurex_macro_output_diagnostic_debuggable`
  - `aurex_macro_output_diagnostic_emission_enabled`
- validation 会拒绝 source item / gate 数不匹配、body 不平衡、query name 漂移、admission identity 漂移、错误 surface
  kind、打开 expansion / compile-time execution / parser consumption / AST mutation / sema-visible generated items /
  standard library / runtime / external process / user generated code。
- M27b validation 还会拒绝 surface gate 缺少 definition-site hygiene gate、顶层 `match` clause 缺少 typed matcher
  gate、hygiene identity 漂移、matcher identity 漂移、matcher kind flags 漂移、typed matcher execution 被打开、
  hygiene resolution 被打开、declared names 被 lookup/export/sema 看见、parser consumption 被打开、AST mutation
  被打开或 user generated code 被打开。
- M27c validation 还会拒绝 macro call source item 与 call-site gate 数不匹配、已声明目标的 call-site 缺少
  matcher-to-call binding、binding 串错 call identity / surface identity / matcher identity、用户 derive schema
  源计数与 gate 数不匹配、schema 串错 derive surface、target schema identity 漂移、call-site / binding / schema
  打开 expansion、matcher execution、compile-time execution、parser consumption、AST mutation、sema-visible generated
  items、standard library、runtime、external process 或 user generated code。
- M27d validation 还会拒绝输出 contract 数量与 M27c binding/schema 输入不一致、缺少 declared-name policy、
  缺少 diagnostic projection、token buffer identity 漂移、output contract identity 漂移、declared-name policy link
  漂移、diagnostic projection link 漂移、generated part 不是 `SourceRole::generated` /
  `ModulePartKind::generated`、打开 generated source text、parser consumption、AST mutation、sema-visible generated
  items、declared name lookup/export/sema visibility、diagnostic parser emission、standard library、runtime、
  external process 或 user generated code。

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
- M27c 新增 `MacroExpansionFactKind::aurex_macro_call_site_admission`。
- M27c 新增 `MacroExpansionFactKind::aurex_macro_matcher_to_call_binding_admission`。
- M27c 新增 `MacroExpansionFactKind::aurex_user_derive_target_schema_admission`。
- M27c 新增对应 policy：
  - `aurex_macro_call_site_admission_v1`
  - `aurex_macro_matcher_to_call_binding_admission_v1`
  - `aurex_user_derive_target_schema_admission_v1`
- M27c 新增 `m27c_macro_expansion_plan_baseline()` 和 `is_valid_m27c_macro_expansion_plan()`。
- M27c plan 是 M27b 13 个 facts 加三类 call-site / matcher binding / user derive target schema facts，共 16 个 facts。
- M27d 新增 `MacroExpansionFactKind::aurex_macro_output_contract_admission`。
- M27d 新增 `MacroExpansionFactKind::aurex_macro_output_declared_name_policy_admission`。
- M27d 新增 `MacroExpansionFactKind::aurex_macro_output_diagnostic_projection_admission`。
- M27d 新增对应 policy：
  - `aurex_macro_output_contract_admission_v1`
  - `aurex_macro_output_declared_name_policy_admission_v1`
  - `aurex_macro_output_diagnostic_projection_admission_v1`
- M27d 新增 `m27d_macro_expansion_plan_baseline()` 和 `is_valid_m27d_macro_expansion_plan()`。
- M27d plan 是 M27c 16 个 facts 加三类 output contract / declared-name policy / diagnostic projection facts，共
  19 个 facts。

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
- 不把 `macro call Name { ... }` 当作 expression 或 statement 语法。
- 不执行 matcher-to-call binding。
- 不实现真实 hygiene resolution。
- 不实现 user derive target schema lowering。
- 不实现 debug trace CLI 或 `--emit-expanded`。
- 不要求或引入标准库。
- 不要求 runtime helper。
- 不启动 external process。
- typed matcher execution is admission-only in M27b。
- definition-site hygiene resolution is admission-only in M27b。
- macro call-site expansion is admission-only in M27c。
- matcher-to-call binding execution is admission-only in M27c。
- user derive target schema is admission-only in M27c。
- macro output parser consumption remains blocked in M27d。
- macro output declared names are hidden from lookup/export/sema in M27d。
- macro output diagnostics are projected but parser emission remains blocked in M27d。
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

M27c 的新增不变量：

- 源中 `ItemKind::macro_call` 数量必须等于 `aurex_macro_call_site_admission_gates` 数量。
- 每个 call-site gate 必须有非零 call token fingerprint、call identity 和 diagnostic anchor identity。
- call-site gate 的 `target_surface_declared` 必须与同模块同名 `AurexMacroSurfaceAdmissionGate` 是否存在一致。
- 已声明目标的 call-site 必须有且只有一个 matcher-to-call binding gate；未声明目标的 call-site 只能保留 blocker。
- binding gate 必须绑定 call identity、surface admission identity 和 recognized matcher identity；没有 recognized matcher
  时必须保持 `binding_admitted=false`。
- 匹配到 `macro derive Name` 的 `#[derive(Name)]` 数量必须等于 user derive target schema gate 数量。
- user derive target schema gate 必须绑定 derive surface admission identity、target schema fingerprint、
  schema identity 和 diagnostic anchor identity。
- M27c 当前只 admit `struct` 和 `enum` derive target schema；字段数、enum case 数、enum payload 数必须来自当前 AST。
- call-site / binding / schema gate 必须保持 `expansion_enabled=false`、
  `compile_time_execution_enabled=false`、`parser_consumption_enabled=false`、`ast_mutated=false`、
  `sema_visible_generated_items=false`、`standard_library_required=false`、`runtime_required=false`、
  `external_process_required=false` 和 `produced_user_generated_code=false`。

M27d 的新增不变量：

- 每个 matcher-to-call binding 和 user derive target schema 必须各有一个 output contract。
- 每个 output contract 必须有一个 declared-name policy gate 和一个 diagnostic projection gate。
- output contract 必须使用 compiler-owned future token buffer identity，并保留 generated `ModulePartKey`。
- generated part 必须保持 `SourceRole::generated` 和 `ModulePartKind::generated`。
- token buffer identity 必须先绑定 source-map / hygiene identity，再参与 output contract identity；不能让 token
  buffer identity 反向依赖 output contract identity。
- declared-name policy 必须绑定 output contract identity、declared-name set fingerprint、hygiene mark 和 diagnostic
  anchor identity。
- diagnostic projection 必须绑定 output contract identity、token buffer identity、source-map identity、hygiene mark 和
  diagnostic anchor identity。
- output contract 必须保持 `compiler_owned_output=true`、`source_map_available=true`、
  `hygiene_mark_available=true`、`diagnostic_projection_available=true` 和
  `declared_name_policy_available=true`。
- output contract 必须保持 `token_buffer_materialized=false`、`generated_source_text=false`、
  `parse_ready=false`、`parser_consumable=false`、`parser_consumption_enabled=false`、
  `ast_mutated=false` 和 `sema_visible_generated_items=false`。
- declared-name policy 必须保持 `declared_name_set_reserved=true`，同时保持 `lookup_visible=false`、
  `export_visible=false` 和 `sema_visible=false`。
- diagnostic projection 必须保持 `debug_projection_available=true`，同时保持
  `diagnostic_emission_enabled=false` 和 `parser_consumable=false`。
- M27d 仍必须保持 `standard_library_required=false`、`runtime_required=false`、
  `external_process_required=false` 和 `produced_user_generated_code=false`。

## 下一步

M27d 已完成 macro output contract admission。建议后续按下面顺序推进：

1. User derive output schema design：为 `macro derive Name` 设计 capability/output 声明、impl/item output 分类和错误定位，
   但仍不执行 lowering。
2. Dry-run parser admission：允许 compiler-owned generated token stream 做受控 parse dry-run，但仍保持 rollback、diagnostic
   shadow 和 no-AST-mutation 闭包。
3. Compile-time execution sandbox：在 const eval / comptime 子集、资源限制、权限模型、deterministic fingerprint 和 diagnostic
   replay 设计完成前，不执行 `macro const`。

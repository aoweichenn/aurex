# Aurex M21g Generated Item Declared Names Stub Contract

日期：2026-06-12
阶段：M21g Generated Item Declared Names Stub Contract
状态：generated item / declared generated name stub facts / no user-generated code

## 目标

M21g 继续沿用 M21d/M21e/M21f 已接入真实 frontend pipeline 的 `macro.expand_items` 边界，不新增 pipeline
stage，不绕过 parser、sema、borrow checker、visibility、trait solver 或 IR verifier。本阶段补齐的是
compiler-owned derive / attached item codegen 真正打开前必须存在的 generated item declaration 和 declared
generated names 契约，而不是开始生成用户代码。

核心目标：

- 每个 parsed item attribute 仍有 `EarlyItemMacroInput`。
- 每个 macro input 仍有 M21f 的 source-map placeholder、hygiene stub 和 trace stub。
- 每个 macro input 新增一个 `GeneratedItemDeclarationStub`。
- 每个 macro input 新增一个 `DeclaredGeneratedNameStub`。
- 新事实绑定 input、attached module part、generated module part、hygiene `declared_name_set` 和 generated fresh mark。
- stub 固定 `attached_item_codegen_declared_names_v1` 策略名。
- stub 只声明 future generated item / future declared name 的稳定身份，不物化 tokens、不 parse、不 merge、不暴露 lookup。
- summary、dump、fingerprint 和 validation 都能发现 generated item / declared name 契约漂移。

## 已实现

新增 frontend macro API：

- `frontend::macro::GeneratedItemDeclarationStub`
- `frontend::macro::DeclaredGeneratedNameStub`
- `is_valid(const GeneratedItemDeclarationStub&)`
- `is_valid(const DeclaredGeneratedNameStub&)`

`EarlyItemExpansionResult` 新增：

- `generated_item_declarations`
- `declared_generated_names`

`EarlyItemExpansionSummary` 新增：

- `generated_item_declaration_stub_count`
- `planned_generated_item_declaration_count`
- `materialized_generated_item_count`
- `declared_generated_name_stub_count`
- `lookup_visible_declared_name_count`
- `export_visible_declared_name_count`

M21g result 名称固定为：

```text
M21g Generated Item Declared Names Stub Contract
```

## Pipeline 边界

M21g 仍运行在同一个 driver profile stage：

```text
module.read / module.lex / module.parse / module.append
macro.expand_items
ast.dump 或 sema.analyze
```

这点保持不变：M21g 不是第二个宏展开 pass，也不是 parser、sema 或 IR lowering 里的特殊后门。它只是把
`EarlyItemExpansionResult` 继续扩展为带 generated item / declared generated name stub contract 的 result。
后续真实 compiler-owned derive / attached item codegen 仍必须沿这个边界推进。

## GeneratedItemDeclarationStub

每个 `GeneratedItemDeclarationStub` 和一个 `EarlyItemMacroInput` 一一对应。当前合法 stub 必须满足：

- `item`、`module`、`part_index`、`attribute_index` 与 input 一致。
- `attached_part` 与 input 一致。
- `generated_part` 指向同 module / source part 的 generated module part placeholder。
- `expansion_origin` 等于 input 的 query-key fingerprint。
- `declared_name_set` 等于对应 `ExpansionHygieneStub::declared_name_set`。
- `declaration_identity` 非空，并由 input identity、generated part、generated part output fingerprint、
  declared-name-set 和 generated item name 共同决定。
- `generated_item_key` 非空，并与 `declaration_identity` 使用不同 marker 生成。
- `declaration_role = attached_item_codegen_declared_names_v1`。
- `generated_item_name` 使用 deterministic internal spelling：
  `__aurex_macro_declared:<module>:<part>:<item>:<attribute>:<attribute_name>`。
- `blocker_reason` 固定为 generated item declaration materialization 在 M21g 被阻塞。
- `planned = true`。
- `materialized_tokens = false`。
- `parsed = false`。
- `merged = false`。
- `sema_visible = false`。
- `produced_user_generated_code = false`。

M21g 的 generated item declaration 只是声明 future item identity。它还不生成 tokens，不构造 AST，不 parse
generated module part，也不把任何生成 item 交给 sema。

## DeclaredGeneratedNameStub

每个 `DeclaredGeneratedNameStub` 和一个 `EarlyItemMacroInput` 一一对应。当前合法 stub 必须满足：

- `item`、`module`、`part_index`、`attribute_index` 与 input 一致。
- `attached_part` 与 input 一致。
- `generated_part` 指向同 module / source part 的 generated module part placeholder。
- `expansion_origin` 等于 input 的 query-key fingerprint。
- `declared_name_set` 等于对应 `ExpansionHygieneStub::declared_name_set`。
- `declared_name_identity` 非空，并由 input identity、generated part、generated part output fingerprint、
  declared-name-set 和 declared name 共同决定。
- `hygiene_mark` 等于对应 `ExpansionHygieneStub::generated_fresh_mark`。
- `declared_name` 等于对应 `GeneratedItemDeclarationStub::generated_item_name`。
- `namespace_kind = item`。
- `blocker_reason` 固定为 declared generated name lookup 在 M21g 被阻塞。
- `lookup_visible = false`。
- `export_visible = false`。
- `sema_visible = false`。
- `produced_user_generated_code = false`。

M21g 的 declared generated names 只是结构化声明名事实。它不参与 name lookup，不进入 export visibility，不对
用户代码可见，也不允许 future generated identifiers 捕获 call-site locals。

## 与 M21a 设计事实的关系

M21a design gate 要求后续宏系统具备：

- `macro_declared_name_fact`
- `attached_macro_declared_name_fact`
- `macro_generated_part_parse_fact`
- `macro_hygiene_mark_fact`
- `macro_expansion_origin_fact`
- `macro_expansion_trace_fact`
- `macro_generated_source_map_fact`

M21g 把其中 declared generated names 的缺口落成 early item expansion result 上的结构化 stub：

- `GeneratedItemDeclarationStub` 对应 future generated item declaration fact。
- `DeclaredGeneratedNameStub` 对应 `macro_declared_name_fact` 和 `attached_macro_declared_name_fact`。
- `generated_part` 继续指向 M21d 的 generated module part placeholder。
- `macro_generated_part_parse_fact` 仍由 M21e 的 `GeneratedModulePartParseMergeStub` 承担。
- `declared_name_set` 和 `hygiene_mark` 继续消费 M21f 的 hygiene stub。

这些事实仍是 stub，不是完整语义执行结果。

## Fingerprint 与 Dump

`early_item_expansion_fingerprint()` 现在混入：

- inputs。
- generated placeholders。
- generated parse / merge stubs。
- source-map placeholders。
- hygiene stubs。
- trace stubs。
- generated item declaration stubs。
- declared generated name stubs。
- summary。
- M21c plan fingerprint。

因此修改 generated item name、declaration identity、generated item key、declared name identity、hygiene mark、
declared-name-set、namespace、role、blocker 或 no-op flags 都会改变 expansion fingerprint，并且 validation 会拒绝
不能按 input / generated part / hygiene 重新计算的漂移。

`dump_early_item_expansion()` 现在会输出：

- `generated_item_declaration_stub` 行。
- `role=attached_item_codegen_declared_names_v1`。
- generated item name。
- planned / materialized / parsed / merged / sema-visible / user-code flags。
- declaration identity。
- declared-name-set。
- generated item key。
- `declared_generated_name_stub` 行。
- `namespace=item`。
- lookup / export / sema visible flags。
- declared name identity。
- hygiene mark。
- blocker reason。

## Validation

M21g validation 会拒绝以下漂移：

- expansion result 名称不是 `M21g Generated Item Declared Names Stub Contract`。
- plan 不是有效 M21c plan。
- generated item declarations 或 declared generated names 与 inputs 数量不一致。
- generated item declaration 与 input 的 item/module/part/attribute/attached part/origin 不一致。
- generated item declaration 没有指向对应 generated module part placeholder。
- generated item declaration 的 declared-name-set 与 hygiene stub 不一致。
- declaration identity 或 generated item key 为空，或无法按 input/generated part/hygiene/name 重算。
- declaration role 不是 `attached_item_codegen_declared_names_v1`。
- generated item name 不是 deterministic internal spelling。
- generated item declaration 声称已经 materialized tokens、parsed、merged、sema-visible 或 produced user code。
- declared generated name 与 input、generated part、hygiene 或 generated item declaration 不一致。
- declared name identity 或 hygiene mark 为空，或无法按 input/generated part/hygiene/name 重算。
- declared generated name namespace 不是 `item`。
- declared generated name 声称已经 lookup-visible、export-visible、sema-visible 或 produced user code。
- generated parse / merge stub、hygiene、trace、source map、summary 或 fingerprint 出现 M21e/M21f 已固定的 drift。

## 当前能做什么

当前内部可用能力：

- parser 仍能保存通用 item attribute token tree。
- `macro.expand_items` 会收集每个 parsed item attribute 的 token-tree fingerprint 和 query-key fingerprint。
- `derive` attribute 仍标记为 `builtin_derive_passthrough`，兼容内建 derive path。
- 非 `derive` item attribute 仍标记为 `blocked_unimplemented_attribute`。
- 每个带 attribute 的 source module part 有 deterministic generated placeholder。
- 每个 generated placeholder 有 deterministic parse / merge stub。
- 每个 macro input 有 deterministic source-map placeholder、hygiene stub 和 trace stub。
- 每个 macro input 有 deterministic generated item declaration stub。
- 每个 macro input 有 deterministic declared generated name stub。
- compiler 内部可以 dump / summarize future generated item identity、declared-name-set、declared name identity、
  hygiene mark、trace identity、generated source map identity 和 diagnostic anchor。
- tests 可以验证 generated item / declared generated name stub contract 和 no-op 边界。

当前用户可见宏能力仍只有内建 `#[derive(Copy, Eq, Hash)]`。M21g 仍不生成用户代码。

## 非目标

M21g 仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- generated module part token materialization。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## 与后续阶段的关系

M21g 让后续阶段可以继续在同一个 result 结构上前进：

- 后续 compiler-owned derive / attached item codegen 可以先消费 `GeneratedItemDeclarationStub` 和
  `DeclaredGeneratedNameStub`，再打开 token materialization。
- 后续 generated module part parse 可把 M21e 的 parse blocker 打开，但仍必须先经过 parser。
- 后续 generated module part merge 可继续使用 `merge_ordering_key` 做 deterministic merge。
- 后续真实 hygiene resolution 可把 declared generated names 接入 lookup，但必须继续携带 hygiene mark。
- 后续真实 diagnostics 可以把 `diagnostic_anchor` 扩展成 expansion stack。
- 后续 `--emit-expanded` 必须先有真实 generated source map 和 stable trace，不能只靠字符串拼接。
- external procedural macro 必须继续等待 sandbox、manifest、permission、implementation fingerprint 和 debug trace
  边界，不应直接跳到执行。

所有后续阶段仍应保持：宏输出必须走普通语言规则，不能绕过 parser、sema、borrow checker、visibility、
trait solver、IR verifier 和 normal diagnostics。

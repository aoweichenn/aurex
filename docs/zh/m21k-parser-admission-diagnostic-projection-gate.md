# Aurex M21k Parser Admission Diagnostic Projection Gate

日期：2026-06-12
阶段：M21k Parser Admission Diagnostic Projection Gate
状态：parser admission diagnostic projection / stable dump surface / parser-blocked / no user-generated code

## 目标

M21k 继续沿用 M21d-M21j 已接入真实 frontend pipeline 的 `macro.expand_items` 边界。本阶段仍不打开
parser consumption，也不 parse / merge generated module part，而是把 M21j parser admission gate 转成可验证的
diagnostic / dump projection。

M21k 的核心目标：

- 每个 parsed item attribute 仍有一个 `EarlyItemMacroInput`。
- 每个 macro input 仍绑定 M21d generated module part placeholder。
- 每个 macro input 仍绑定 M21e generated module part parse / merge stub。
- 每个 macro input 仍绑定 M21f hygiene、trace、source-map identities。
- 每个 macro input 仍绑定 M21g generated item declaration 和 declared generated name identities。
- 每个 macro input 仍绑定 M21h token materialization admission。
- 每个 macro input 仍绑定 M21i generated token buffer 和 optional generated token records。
- 每个 macro input 仍绑定 M21j `GeneratedTokenParserAdmissionGateStub`。
- 每个 macro input 新增一个 `ParserAdmissionDiagnosticProjectionStub`。
- diagnostic projection 明确区分 token buffer admission blocker 和 generated module part parse blocker。
- diagnostic projection 固定 source anchor、token-tree anchor、blocker category、gate identity、trace identity、
  source-map identity 和 hygiene mark。
- diagnostic projection 预留 future `--emit-expanded`、debug trace projection 和 source-map projection 状态，但全部固定为
  unavailable。
- summary、dump、fingerprint 和 validation 都能发现 diagnostic projection 与 input、parser gate、parse config、
  generated buffer identity、token buffer、source-map、hygiene 或 trace 的绑定漂移。

M21k 仍不生成用户代码，也不让 parser 消费 generated token buffer。

## 已实现

新增 frontend macro API：

- `frontend::macro::ParserAdmissionDiagnosticProjectionStub`
- `is_valid(const ParserAdmissionDiagnosticProjectionStub&)`

`EarlyItemExpansionResult` 新增：

- `parser_admission_diagnostics`

`EarlyItemExpansionSummary` 新增：

- `parser_admission_diagnostic_stub_count`
- `parser_admission_diagnostic_blocked_count`
- `derive_parser_admission_diagnostic_count`
- `empty_parser_admission_diagnostic_count`
- `emit_expanded_projection_available_count`
- `parser_admission_debug_trace_projection_count`
- `parser_admission_source_map_projection_count`

M21k result 名称固定为：

```text
M21k Parser Admission Diagnostic Projection Gate
```

## Pipeline 边界

M21k 仍运行在同一个 driver profile stage：

```text
module.read / module.lex / module.parse / module.append
macro.expand_items
ast.dump 或 sema.analyze
```

这点保持不变：M21k 不是 parser 的新入口，也不是 AST mutation pass。它只把 parser admission 阻塞原因投影成
compiler-owned facts，便于后续 query/cache/tooling/debug surface 统一消费。后续如果要真正 parse generated module
part，必须先把 `parser_admitted=false`、`parse_ready=false`、`parser_consumable=false` 这组 gate 状态升级为可验证的
admitted 状态，并补齐 parser recovery、diagnostic range remap、source map、hygiene 和 merge 语义。

## ParserAdmissionDiagnosticProjectionStub

每个 `ParserAdmissionDiagnosticProjectionStub` 和一个 `EarlyItemMacroInput` 一一对应。它消费并绑定以下事实：

- M21d `GeneratedModulePartPlaceholder::generated_part`
- M21e `GeneratedModulePartParseMergeStub::generated_buffer_identity`
- M21e `GeneratedModulePartParseMergeStub::parse_config_fingerprint`
- M21f `ExpansionTraceStub::trace_identity`
- M21f `ExpansionTraceStub::generated_source_map_identity`
- M21f `ExpansionTraceStub::diagnostic_anchor`
- M21h/M21i `GeneratedTokenBufferStub::token_plan_identity`
- M21h/M21i `GeneratedTokenBufferStub::token_buffer_identity`
- M21i `GeneratedTokenBufferStub::materialization_identity`
- M21j `GeneratedTokenParserAdmissionGateStub::parse_gate_identity`
- source-map identity
- hygiene mark
- token count

字段固定如下：

- `diagnostic_policy = parser_admission_blocked_diagnostic_projection_v1`
- `generated_part_parse_blocker = generated module part parse remains blocked before parser admission diagnostics in M21k`
- `parser_admitted = false`
- `parse_ready = false`
- `parser_consumable = false`
- `generated_part_parsed = false`
- `generated_part_merged = false`
- `emit_expanded_available = false`
- `debug_trace_available = false`
- `source_map_available = false`
- `produced_user_generated_code = false`

`diagnostic_anchor_identity` 会混入 input identity、attribute source range、token-tree source range、M21j
`parse_gate_identity`、M21f `trace_identity` 和 M21f `diagnostic_anchor`。它用于稳定定位“这个 parser admission
diagnostic 应该挂在哪个源位置”。

`diagnostic_identity` 会混入 input identity、generated part、M21j parse gate identity、diagnostic anchor、token
plan、token buffer、materialization identity、M21e generated buffer identity、M21e parse config、source-map
identity、hygiene mark、trace identity、diagnostic policy、blocker category、两类 blocker、user message、debug
projection name、token count 和所有 blocked flags。它用于检测 diagnostic projection drift。

## 非 derive 空 buffer projection

非 `derive` item attribute 仍保持空 generated token buffer。对应 diagnostic projection 固定为：

- `blocker_category = empty_token_buffer_parser_admission_blocked`
- `token_buffer_blocker = empty or non-derive generated token buffer parser admission remains blocked in M21j`
- `user_message = generated token buffer is empty and parser admission remains blocked in M21k`
- `token_count = 0`
- `token_buffer_materialized = false`
- `token_records_available = false`
- `parser_admitted = false`

这表示非 `derive` attribute 当前只有 parsed token-tree input 和 compiler-owned no-op facts；它没有 generated token
records，也没有 parser-consumable token stream。

## derive prototype projection

`derive` item attribute 在 M21i 已有 compiler-owned prototype token records，M21j 已有 parser admission gate。
M21k 不改变这些 records，也不打开 parser admission，只把阻塞状态投影为：

- `blocker_category = derive_token_buffer_parser_admission_blocked`
- `token_buffer_blocker = compiler-owned derive generated token buffer parser admission remains blocked in M21j`
- `user_message = generated derive token buffer is compiler-owned but parser admission remains blocked in M21k`
- `token_count = derive attribute token_tree token count + 2`
- `token_buffer_materialized = true`
- `token_records_available = true`
- `parser_admitted = false`

这表示当前 compiler 可以证明 derive prototype token records 已存在，但这些 records 仍只是 provenance / debug /
future-codegen facts，不是 parser 可消费 token stream。

## 当前能做什么

M21k 当前能做到：

- 解析并索引 item attributes。
- 保留 `#[derive(...)]` 的 attribute token tree。
- 对 `derive` 输入产生 compiler-owned generated token records。
- 对非 `derive` 输入保持 empty generated token buffer。
- 为每个 macro input 产生 deterministic parser admission gate。
- 为每个 macro input 产生 deterministic parser admission diagnostic projection。
- 在 diagnostic projection 中区分 token buffer admission blocker 和 generated module part parse blocker。
- 在 diagnostic projection 中固定 primary source anchor 和 token-tree anchor。
- 在 diagnostic projection 中固定 blocker category、user message 和 debug projection name。
- 在 dump 中显示 diagnostic policy、category、anchors、token records availability、parser flags、emit/debug/source-map
  availability、diagnostic identity 和 trace identity。
- 在 summary 中统计 parser admission diagnostics、blocked diagnostics、derive / empty categories 和 future projection
  availability counts。
- 用 fingerprint 追踪 diagnostic identity、anchor、category、debug projection name、parse config、generated buffer
  identity、token buffer identity、source map、hygiene 和 trace 的依赖。
- 用 validation 拒绝 diagnostic projection 漂移。

## 当前不能做什么

M21k 仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- generated module part merge ordering execution。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## Validation Contract

M21k validation 会拒绝以下漂移：

- expansion result 名称不是 `M21k Parser Admission Diagnostic Projection Gate`。
- `parser_admission_diagnostics.size()` 和 `inputs.size()` 不一致。
- diagnostic 不绑定对应 input 的 `item`、`module`、`part_index`、`attribute_index` 或 `attached_part`。
- diagnostic 不绑定对应 generated module part。
- diagnostic 的 `primary_anchor` 不等于 input attribute range。
- diagnostic 的 `token_tree_anchor` 不等于 input token tree range。
- diagnostic 的 `parse_gate_identity` 不等于 M21j parser admission gate。
- diagnostic 的 `diagnostic_anchor_identity` 不能按 M21k anchor rule 重算。
- diagnostic 的 `diagnostic_identity` 不能按 M21k identity rule 重算。
- diagnostic 的 `token_plan_identity`、`token_buffer_identity` 或 `materialization_identity` 不等于对应 gate / buffer。
- diagnostic 的 `generated_buffer_identity` 不等于 M21e parse / merge stub 的 identity。
- diagnostic 的 `parse_config_fingerprint` 不等于 M21e parse / merge stub 的 parse config。
- diagnostic 的 `source_map_identity` 或 `hygiene_mark` 不等于对应 token buffer。
- diagnostic 的 `trace_identity` 不等于 M21f trace stub。
- diagnostic policy 不是 `parser_admission_blocked_diagnostic_projection_v1`。
- blocker category 不是 M21k 的 derive / empty category。
- token buffer blocker 不是 M21j 的 derive / non-derive parser admission blocker。
- generated part parse blocker 不是 M21k 的 generated module part parse blocker。
- user message 不是 M21k 的 derive / non-derive message。
- debug projection name 不能按 input identity 重算。
- `token_buffer_materialized` 和 token count / category 不一致。
- `token_records_available` 和 token count / category 不一致。
- `parser_admitted = true`。
- `parse_ready = true`。
- `parser_consumable = true`。
- `generated_part_parsed = true`。
- `generated_part_merged = true`。
- `emit_expanded_available = true`。
- `debug_trace_available = true`。
- `source_map_available = true`。
- `produced_user_generated_code = true`。

## 测试覆盖

新增和更新的测试集中在：

- `tests/gtest/frontend/macro/early_item_expansion_tests.cpp`

覆盖内容包括：

- M21k result name、summary、dump 和 fingerprint。
- 每个 macro input 都有 parser admission diagnostic projection。
- 非 `derive` projection 保持 empty category / no records / parser-blocked。
- `derive` projection 保持 derive category / records available / parser-blocked。
- projection 绑定 M21e generated buffer identity 和 parse config。
- projection 绑定 M21i token buffer identity、materialization identity、source-map identity 和 hygiene mark。
- projection 绑定 M21j parse gate identity。
- projection 绑定 M21f trace identity 和 source anchors。
- validation 拒绝缺失 projection。
- validation 拒绝 diagnostic identity、diagnostic anchor、parse gate identity、parse config、generated buffer
  identity、token buffer identity、materialization identity、source-map identity、hygiene mark、trace identity、
  policy、category、blocker、message、debug projection name、source anchors、token count 或 availability flags 漂移。
- validation 拒绝 parser admitted、parse ready、parser consumable、generated part parsed / merged、`--emit-expanded`
  projection、debug trace projection、source-map projection 和 produced user code。
- fingerprint 会追踪 diagnostic identity、category、debug projection name 和 source anchor。

## 下一步

下一步建议继续 M21l：仍保持 parser-blocked / no user-code，做 parser admission report / query projection
hardening。M21l 应把 M21k diagnostic projection 汇总成 tooling/query 可复用的 report surface，固定 report identity、
category totals、source anchor ordering 和 negative validation matrix；仍不打开 parser consumption，不执行 external
procedural macro，不生成 source text，不实现标准库或 runtime helper。

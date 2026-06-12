# Aurex M21j Generated Token Parser Admission Gate

日期：2026-06-12
阶段：M21j Generated Token Parser Admission Gate
状态：compiler-owned generated token parser admission gate / generated module part parse admission guard / parser-blocked / no user-generated code

## 目标

M21j 继续沿用 M21d/M21e/M21f/M21g/M21h/M21i 已接入真实 frontend pipeline 的
`macro.expand_items` 边界。本阶段不打开 parser consumption，而是把“generated token buffer 是否允许进入
parser”从 token materialization 层单独拆成结构化 gate。

M21j 的核心目标：

- 每个 parsed item attribute 仍有 `EarlyItemMacroInput`。
- 每个 macro input 仍绑定 M21d generated module part placeholder。
- 每个 macro input 仍绑定 M21e generated module part parse / merge stub。
- 每个 macro input 仍绑定 M21f hygiene、trace、source-map identities。
- 每个 macro input 仍绑定 M21g generated item declaration 和 declared generated name identities。
- 每个 macro input 仍绑定 M21h token materialization admission。
- 每个 macro input 仍绑定 M21i generated token buffer 和 optional generated token records。
- 每个 macro input 新增一个 `GeneratedTokenParserAdmissionGateStub`。
- gate 会显式记录 token buffer 是否 materialized、token records 是否 available。
- gate 会显式记录 parser admission、parse-ready、parser-consumable、generated part parsed / merged、sema-visible
  和 produced-user-code 都仍为 false。
- summary、dump、fingerprint 和 validation 都能发现 parser gate 与 input、generated part、parse config、
  generated buffer identity、token buffer、source-map 或 hygiene 的绑定漂移。

M21j 仍不生成用户代码，也不让 parser 消费 generated token buffer。

## 已实现

新增 frontend macro API：

- `frontend::macro::GeneratedTokenParserAdmissionGateStub`
- `is_valid(const GeneratedTokenParserAdmissionGateStub&)`

`EarlyItemExpansionResult` 新增：

- `parser_admission_gates`

`EarlyItemExpansionSummary` 新增：

- `parser_admission_gate_stub_count`
- `compiler_owned_parser_admission_gate_count`
- `token_record_available_gate_count`
- `parser_blocked_token_buffer_count`
- `parser_admitted_token_buffer_count`

M21j result 名称固定为：

```text
M21j Generated Token Parser Admission Gate
```

## Pipeline 边界

M21j 仍运行在同一个 driver profile stage：

```text
module.read / module.lex / module.parse / module.append
macro.expand_items
ast.dump 或 sema.analyze
```

这点保持不变：M21j 不是 parser 的新入口，也不是 AST mutation pass。它只是把 future parser consumption 的
admission precondition 提前固化为 compiler-owned facts。后续如果要真正 parse generated module part，必须先让
这个 gate 从 `parser_admitted=false` 过渡到可验证的 admitted 状态，并补齐对应 parser、diagnostic、source-map、
hygiene 和 merge 语义。

## GeneratedTokenParserAdmissionGateStub

每个 `GeneratedTokenParserAdmissionGateStub` 和一个 `EarlyItemMacroInput` 一一对应。它消费并绑定以下事实：

- M21d `GeneratedModulePartPlaceholder::generated_part`
- M21e `GeneratedModulePartParseMergeStub::generated_buffer_identity`
- M21e `GeneratedModulePartParseMergeStub::parse_config_fingerprint`
- M21h/M21i `GeneratedTokenBufferStub::token_plan_identity`
- M21h/M21i `GeneratedTokenBufferStub::token_buffer_identity`
- M21i `GeneratedTokenBufferStub::materialization_identity`
- M21f `source_map_identity`
- M21f `hygiene_mark`
- token stream name
- token count

字段固定如下：

- `parser_gate_policy = compiler_owned_generated_token_parser_admission_gate_v1`
- `compiler_owned = true`
- `parser_admitted = false`
- `parse_ready = false`
- `parser_consumable = false`
- `generated_source_text = false`
- `generated_part_parsed = false`
- `generated_part_merged = false`
- `sema_visible = false`
- `produced_user_generated_code = false`

`parse_gate_identity` 会混入 input identity、generated part、M21e generated buffer identity、M21e parse config、
token plan identity、token buffer identity、materialization identity、source-map identity、hygiene mark、token
stream name、gate policy、blocker reason、token count、materialized state 和 token-record availability。它用于区分
“token buffer 已经存在”与“该 token buffer 是否允许进入 parser”两个不同事实。

## 非 derive 空 buffer gate

非 `derive` item attribute 仍保持空 generated token buffer。对应 parser admission gate 固定为：

- `token_count = 0`
- `token_buffer_materialized = false`
- `token_records_available = false`
- `parser_admitted = false`
- `parse_ready = false`
- `parser_consumable = false`
- `blocker_reason = empty or non-derive generated token buffer parser admission remains blocked in M21j`

这表示非 `derive` attribute 目前只有 token-tree input 和 no-op compiler facts；它没有 generated token records，也没有
进入 parser 的候选 token stream。

## derive prototype buffer gate

`derive` item attribute 在 M21i 已有 compiler-owned prototype token records。M21j 不改变这些 records，只在 parser
admission 层固定额外事实：

- `token_count = derive attribute token_tree token count + 2`
- `token_buffer_materialized = true`
- `token_records_available = true`
- `parser_admitted = false`
- `parse_ready = false`
- `parser_consumable = false`
- `blocker_reason = compiler-owned derive generated token buffer parser admission remains blocked in M21j`

这表示当前 compiler 可以证明 derive prototype token records 已存在，但这些 records 仍只是 provenance / debug /
future-codegen facts，不是 parser 可消费 token stream。

## 当前能做什么

M21j 当前能做到：

- 解析并索引 item attributes。
- 保留 `#[derive(...)]` 的 attribute token tree。
- 对 `derive` 输入产生 compiler-owned generated token records。
- 对非 `derive` 输入保持 empty generated token buffer。
- 为每个 macro input 产生 deterministic parser admission gate。
- 在 dump 中显示 token buffer 是否 materialized、token records 是否 available、parser 是否 admitted。
- 在 summary 中统计 parser admission gates、available record gates、blocked/admitted token buffers。
- 用 fingerprint 追踪 parser gate 对 parse config、generated buffer identity、token buffer identity、source map 和
  hygiene 的依赖。
- 用 validation 拒绝 parser gate 漂移。

## 当前不能做什么

M21j 仍不实现：

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

M21j validation 会拒绝以下漂移：

- expansion result 名称不是 `M21j Generated Token Parser Admission Gate`。
- `parser_admission_gates.size()` 和 `inputs.size()` 不一致。
- gate 不绑定对应 input 的 `item`、`module`、`part_index`、`attribute_index` 或 `attached_part`。
- gate 不绑定对应 generated module part。
- gate 的 `generated_buffer_identity` 不等于 M21e parse / merge stub 的 identity。
- gate 的 `parse_config_fingerprint` 不等于 M21e parse / merge stub 的 parse config。
- gate 的 `token_plan_identity`、`token_buffer_identity` 或 `materialization_identity` 不等于对应 token buffer。
- gate 的 `source_map_identity` 或 `hygiene_mark` 不等于对应 token buffer。
- gate 的 `parse_gate_identity` 不能按 M21j identity rule 重算。
- gate policy 不是 `compiler_owned_generated_token_parser_admission_gate_v1`。
- blocker reason 不是 M21j 的 derive / non-derive parser admission blocker。
- `token_buffer_materialized` 和 token count 不一致。
- `token_records_available` 和 token count 不一致。
- `parser_admitted = true`。
- `parse_ready = true`。
- `parser_consumable = true`。
- `generated_source_text = true`。
- `generated_part_parsed = true`。
- `generated_part_merged = true`。
- `sema_visible = true`。
- `produced_user_generated_code = true`。

## 测试覆盖

新增和更新的测试集中在：

- `tests/gtest/frontend/macro/early_item_expansion_tests.cpp`

覆盖内容包括：

- M21j result name、summary、dump 和 fingerprint。
- 每个 macro input 都有 parser admission gate。
- 非 `derive` gate 保持 empty / no records / parser-blocked。
- `derive` gate 保持 materialized / records available / parser-blocked。
- gate 绑定 M21e generated buffer identity 和 parse config。
- gate 绑定 M21i token buffer identity、materialization identity、source-map identity 和 hygiene mark。
- validation 拒绝缺失 gate。
- validation 拒绝 parse gate identity、parse config、generated buffer identity、token buffer identity、
  materialization identity、source-map identity、hygiene mark、policy、blocker、token count 或 stream name 漂移。
- validation 拒绝 parser admitted、parse ready、parser consumable、generated source text、generated part parsed /
  merged、sema visible 和 produced user code。
- fingerprint 会追踪 parser gate identity、parse config、policy 和 token record availability。

## 下一步

下一步建议继续 M21k：在仍不打开 parser consumption 的前提下，把 parser admission gate 的 diagnostic / dump
projection 做成更接近 future user-facing 的报告模型，特别是：

- 区分 token buffer admission blocker 和 generated module part parse blocker。
- 为 future `--emit-expanded` 和 debug trace 预留稳定 query projection。
- 固定 parser admission diagnostic 的 source anchor、gate identity 和 blocker category。
- 继续保持 no standard library、no runtime helper、no external procedural macro、no user-generated code。


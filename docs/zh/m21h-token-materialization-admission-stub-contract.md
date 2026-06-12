# Aurex M21h Token Materialization Admission Stub Contract

日期：2026-06-12
阶段：M21h Token Materialization Admission Stub Contract
状态：compiler-owned token materialization admission / empty token buffer stub / no user-generated code

## 目标

M21h 继续沿用 M21d/M21e/M21f/M21g 已接入真实 frontend pipeline 的 `macro.expand_items` 边界，不新增
pipeline stage，不绕过 parser、sema、borrow checker、visibility、trait solver 或 IR verifier。本阶段补齐的是
compiler-owned derive / attached item codegen 真正开始物化 tokens 之前必须存在的 admission fact 和 empty token
buffer contract，而不是开始生成宏输出。

核心目标：

- 每个 parsed item attribute 仍有 `EarlyItemMacroInput`。
- 每个 macro input 仍绑定 M21e generated module part parse/merge stub。
- 每个 macro input 仍绑定 M21f hygiene、trace、source-map identities。
- 每个 macro input 仍绑定 M21g generated item declaration 和 declared generated name identities。
- 每个 macro input 新增一个 `TokenMaterializationAdmissionStub`。
- 每个 macro input 新增一个 `GeneratedTokenBufferStub`。
- admission 固定 `compiler_owned_attached_item_token_materialization_admission_v1` 策略名。
- generated token buffer 固定 `compiler_owned_empty_token_stream` kind。
- token plan / token buffer identities 可从 input、generated part、hygiene、trace、generated item declaration 和
  declared generated name 一一重算。
- stub 只声明 future compiler-owned token materialization admission，不生成 tokens、不生成 source text、不让 parser
  消费、不 parse、不 merge、不进入 sema。
- summary、dump、fingerprint 和 validation 都能发现 token materialization admission / buffer contract 漂移。

## 已实现

新增 frontend macro API：

- `frontend::macro::TokenMaterializationAdmissionStub`
- `frontend::macro::GeneratedTokenBufferStub`
- `is_valid(const TokenMaterializationAdmissionStub&)`
- `is_valid(const GeneratedTokenBufferStub&)`

`EarlyItemExpansionResult` 新增：

- `token_materialization_admissions`
- `generated_token_buffers`

`EarlyItemExpansionSummary` 新增：

- `token_materialization_admission_stub_count`
- `compiler_owned_admission_count`
- `admitted_token_materialization_count`
- `materialized_token_admission_count`
- `generated_token_buffer_stub_count`
- `empty_generated_token_buffer_count`
- `materialized_token_buffer_count`
- `generated_source_text_count`
- `parse_ready_token_buffer_count`

M21h result 名称固定为：

```text
M21h Token Materialization Admission Stub Contract
```

## Pipeline 边界

M21h 仍运行在同一个 driver profile stage：

```text
module.read / module.lex / module.parse / module.append
macro.expand_items
ast.dump 或 sema.analyze
```

这点保持不变：M21h 不是第二个宏展开 pass，也不是 parser、sema 或 IR lowering 里的特殊后门。它只是把
`EarlyItemExpansionResult` 继续扩展为带 compiler-owned token materialization admission / empty token buffer
stub contract 的 result。后续真实 compiler-owned derive / attached item codegen 仍必须沿这个边界推进。

## TokenMaterializationAdmissionStub

每个 `TokenMaterializationAdmissionStub` 和一个 `EarlyItemMacroInput` 一一对应。当前合法 stub 必须满足：

- `item`、`module`、`part_index`、`attribute_index` 与 input 一致。
- `attached_part` 与 input 一致。
- `generated_part` 指向同 module / source part 的 generated module part placeholder。
- `expansion_origin` 等于 input 的 query-key fingerprint。
- `declaration_identity` 等于对应 `GeneratedItemDeclarationStub::declaration_identity`。
- `generated_item_key` 等于对应 `GeneratedItemDeclarationStub::generated_item_key`。
- `declared_name_set` 等于对应 `ExpansionHygieneStub::declared_name_set`。
- `declared_name_identity` 等于对应 `DeclaredGeneratedNameStub::declared_name_identity`。
- `hygiene_mark` 等于对应 `ExpansionHygieneStub::generated_fresh_mark` 和
  `DeclaredGeneratedNameStub::hygiene_mark`。
- `source_map_identity` 等于对应 `ExpansionTraceStub::generated_source_map_identity`。
- `trace_identity` 等于对应 `ExpansionTraceStub::trace_identity`。
- `token_plan_identity` 非空，并由 input identity、generated part、generated part output fingerprint、
  declaration identity、generated item key、declared-name-set、declared name identity、hygiene mark、trace identity、
  source-map identity 和 token stream name 共同决定。
- `token_buffer_identity` 非空，并由 input identity、generated part、generated part output fingerprint、
  token plan identity、hygiene mark、source-map identity、token buffer kind 和 token stream name 共同决定。
- `admission_policy = compiler_owned_attached_item_token_materialization_admission_v1`。
- `token_stream_name` 使用 deterministic internal spelling：
  `m21h-token-stream:<module>:<part>:<item>:<attribute>:<attribute_name>`。
- `blocker_reason` 固定为 compiler-owned macro token materialization 在 M21h 被阻塞。
- `compiler_owned = true`。
- `admitted = true`。
- `materialized_tokens = false`。
- `generated_source_text = false`。
- `parse_ready = false`。
- `external_process_required = false`。
- `standard_library_required = false`。
- `runtime_required = false`。
- `produced_user_generated_code = false`。

这里的 `admitted = true` 只表示“这个 macro input 已进入 future compiler-owned token materialization admission
事实集”，不是 tokens 已经存在。M21h 仍没有任何真实 token stream 或 generated source text。

## GeneratedTokenBufferStub

每个 `GeneratedTokenBufferStub` 和一个 `EarlyItemMacroInput` 一一对应。当前合法 stub 必须满足：

- `item`、`module`、`part_index`、`attribute_index` 与 input 一致。
- `attached_part` 与 input 一致。
- `generated_part` 指向同 module / source part 的 generated module part placeholder。
- `token_plan_identity` 等于对应 admission 的 token plan identity。
- `token_buffer_identity` 等于对应 admission 的 token buffer identity。
- `source_map_identity` 等于对应 trace 的 generated source map identity。
- `hygiene_mark` 等于对应 hygiene 的 generated fresh mark。
- `token_stream_name` 等于对应 admission 的 token stream name。
- `token_buffer_kind = compiler_owned_empty_token_stream`。
- `blocker_reason` 固定为 generated token buffer 在 M21h 仍为空且 parser-blocked。
- `token_count = 0`。
- `empty = true`。
- `materialized_tokens = false`。
- `generated_source_text = false`。
- `parser_consumable = false`。
- `produced_user_generated_code = false`。

M21h 的 generated token buffer 只是未来 token stream 的稳定占位 fact。它不能被 parser 消费，也不代表存在
generated module part source buffer。

## 与 M21a 设计事实的关系

M21a design gate 要求后续宏系统具备：

- `macro_token_materialization_fact`
- `macro_generated_token_buffer_fact`
- `macro_generated_part_parse_fact`
- `macro_hygiene_mark_fact`
- `macro_expansion_origin_fact`
- `macro_expansion_trace_fact`
- `macro_generated_source_map_fact`
- `macro_declared_name_fact`

M21h 把其中 token materialization admission 和 generated token buffer 的缺口落成 early item expansion result
上的结构化 stub：

- `TokenMaterializationAdmissionStub` 对应 future `macro_token_materialization_fact` 的准入层。
- `GeneratedTokenBufferStub` 对应 future `macro_generated_token_buffer_fact` 的 empty buffer 层。
- `macro_generated_part_parse_fact` 仍由 M21e 的 `GeneratedModulePartParseMergeStub` 承担。
- `declared_name_set`、`hygiene_mark`、`trace_identity` 和 `source_map_identity` 继续消费 M21f facts。
- `declaration_identity`、`generated_item_key` 和 `declared_name_identity` 继续消费 M21g facts。

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
- token materialization admission stubs。
- generated token buffer stubs。
- summary。
- M21c plan fingerprint。

因此修改 token stream name、admission policy、token plan identity、token buffer identity、source-map identity、
trace identity、hygiene mark、declaration identity、generated item key、declared name identity、token buffer kind、
blocker 或 no-op flags 都会改变 expansion fingerprint，并且 validation 会拒绝不能按 input / generated part /
hygiene / trace / generated item / declared generated name 重新计算的漂移。

`dump_early_item_expansion()` 现在会输出：

- `token_materialization_admission_stub` 行。
- `policy=compiler_owned_attached_item_token_materialization_admission_v1`。
- token stream name。
- compiler-owned / admitted / materialized / generated source text / parse-ready flags。
- external process / standard library / runtime blocker flags。
- declaration identity、generated item key、declared name identity、hygiene mark、source-map identity、trace identity。
- token plan identity。
- token buffer identity。
- `generated_token_buffer_stub` 行。
- `kind=compiler_owned_empty_token_stream`。
- `token_count=0`。
- empty / materialized / parser-consumable flags。
- blocker reason。

## Validation

M21h validation 会拒绝以下漂移：

- expansion result 名称不是 `M21h Token Materialization Admission Stub Contract`。
- plan 不是有效 M21c plan。
- token materialization admissions 或 generated token buffers 与 inputs 数量不一致。
- admission 与 input 的 item/module/part/attribute/attached part/origin 不一致。
- admission 没有指向对应 generated module part placeholder。
- admission 没有绑定对应 M21g declaration identity / generated item key / declared name identity。
- admission 没有绑定对应 M21f declared-name-set / hygiene mark / source-map identity / trace identity。
- token plan identity 或 token buffer identity 为空，或无法按 input/generated part/hygiene/trace/generated item/name
  重算。
- admission policy 不是 `compiler_owned_attached_item_token_materialization_admission_v1`。
- token stream name 不是 deterministic internal spelling。
- admission 声称不是 compiler-owned，或未 admitted。
- admission 声称已经 materialized tokens、generated source text、parse-ready、需要 external process、需要标准库、
  需要 runtime 或 produced user code。
- generated token buffer 与 input、generated part、hygiene、trace 或 admission 不一致。
- token buffer kind 不是 `compiler_owned_empty_token_stream`。
- generated token buffer token_count 非 0，或 empty 不是 true。
- generated token buffer 声称已经 materialized tokens、generated source text、parser-consumable 或 produced user code。
- generated parse / merge stub、hygiene、trace、source map、generated item declaration、declared generated name、
  summary 或 fingerprint 出现 M21e/M21f/M21g 已固定的 drift。

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
- 每个 macro input 有 deterministic token materialization admission stub。
- 每个 macro input 有 deterministic empty generated token buffer stub。
- compiler 内部可以 dump / summarize future generated item identity、declared name identity、token plan identity、
  token buffer identity、source-map identity、hygiene mark、trace identity 和 diagnostic anchor。
- tests 可以验证 token materialization admission / generated token buffer stub contract 和 no-op 边界。

当前用户可见宏能力仍只有内建 `#[derive(Copy, Eq, Hash)]`。M21h 仍不生成用户代码。

## 非目标

M21h 仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- generated module part real token materialization。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## 与后续阶段的关系

M21h 让后续阶段可以继续在同一个 result 结构上前进：

- 后续 compiler-owned derive / attached item codegen 可以先消费 `TokenMaterializationAdmissionStub`，再把
  `GeneratedTokenBufferStub` 从 empty stub 转成真实 compiler-owned token buffer。
- 后续 generated module part parse 必须先看到 parser-consumable token buffer，再打开 M21e 的 parse blocker。
- 后续 generated module part merge 可继续使用 `merge_ordering_key` 做 deterministic merge。
- 后续真实 hygiene resolution 可把 generated tokens 的 identifiers 绑定到 M21f/M21g 的 hygiene/name facts。
- 后续真实 diagnostics 可以把 token buffer source ranges 接到 `diagnostic_anchor` 和 expansion trace。
- 后续 `--emit-expanded` 必须先有真实 generated source map 和 stable trace，不能只靠字符串拼接。
- external procedural macro 必须继续等待 sandbox、manifest、permission、implementation fingerprint 和 debug trace
  边界，不应直接跳到执行。

所有后续阶段仍应保持：宏输出必须走普通语言规则，不能绕过 parser、sema、borrow checker、visibility、
trait solver、IR verifier 和 normal diagnostics。

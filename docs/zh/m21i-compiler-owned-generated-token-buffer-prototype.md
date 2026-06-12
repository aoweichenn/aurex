# Aurex M21i Compiler-Owned Generated Token Buffer Prototype

日期：2026-06-12
阶段：M21i Compiler-Owned Generated Token Buffer Prototype
状态：compiler-owned derive token buffer prototype / generated token record facts / parser-blocked / no user-generated code

## 目标

M21i 继续沿用 M21d/M21e/M21f/M21g/M21h 已接入真实 frontend pipeline 的 `macro.expand_items`
边界，不新增 pipeline stage，不绕过 parser、sema、borrow checker、visibility、trait solver 或 IR verifier。
本阶段第一次让 compiler-owned token buffer 不再全是空占位，但范围严格限制在内建 `derive` 输入的
prototype token records。

核心目标：

- 每个 parsed item attribute 仍有 `EarlyItemMacroInput`。
- 每个 macro input 仍绑定 M21e generated module part parse/merge stub。
- 每个 macro input 仍绑定 M21f hygiene、trace、source-map identities。
- 每个 macro input 仍绑定 M21g generated item declaration 和 declared generated name identities。
- 每个 macro input 仍有 `TokenMaterializationAdmissionStub`。
- 每个 macro input 仍有 `GeneratedTokenBufferStub`。
- `derive` 输入的 admission 允许记录 `materialized_tokens = true`，但只表示 compiler-owned prototype token
  records 已存在，不表示用户代码已生成。
- `derive` 输入的 generated token buffer 使用
  `compiler_owned_builtin_derive_token_stream_prototype` kind。
- `derive` 输入的 generated token buffer 使用
  `compiler_owned_builtin_derive_token_producer_prototype_v1` producer policy。
- `derive` 输入新增结构化 `GeneratedTokenRecord` facts。
- 非 `derive` item attribute 仍使用 `compiler_owned_empty_token_stream`，不产生 token record。
- 所有 generated token records 都必须是 compiler-owned、parser-invisible、non-user-code。
- summary、dump、fingerprint 和 validation 都能发现 token materialization、buffer、record、source-map 或 hygiene
  绑定漂移。

M21i 仍不生成用户代码，也不让 parser 消费 generated token buffer。

## 已实现

新增 frontend macro API：

- `frontend::macro::GeneratedTokenRecord`
- `is_valid(const GeneratedTokenRecord&)`

扩展已有 API：

- `frontend::macro::TokenMaterializationAdmissionStub`
- `frontend::macro::GeneratedTokenBufferStub`
- `is_valid(const TokenMaterializationAdmissionStub&)`
- `is_valid(const GeneratedTokenBufferStub&)`

`EarlyItemExpansionResult` 新增：

- `generated_token_records`

`GeneratedTokenBufferStub` 新增：

- `materialization_identity`
- `token_producer_policy`

`EarlyItemExpansionSummary` 新增：

- `compiler_owned_token_buffer_count`
- `generated_token_record_count`
- `compiler_owned_generated_token_record_count`
- `parser_visible_generated_token_count`

M21i result 名称固定为：

```text
M21i Compiler-Owned Generated Token Buffer Prototype
```

## Pipeline 边界

M21i 仍运行在同一个 driver profile stage：

```text
module.read / module.lex / module.parse / module.append
macro.expand_items
ast.dump 或 sema.analyze
```

这点保持不变：M21i 不是第二个宏展开 pass，也不是 parser、sema 或 IR lowering 里的特殊后门。它只是把
`EarlyItemExpansionResult` 继续扩展为带 compiler-owned generated token buffer prototype 的 result。后续真实
compiler-owned derive / attached item codegen 仍必须沿这个边界推进。

## TokenMaterializationAdmissionStub

每个 `TokenMaterializationAdmissionStub` 和一个 `EarlyItemMacroInput` 一一对应。M21i 保留 M21h 的 admission
身份模型：

- `admission_policy = compiler_owned_attached_item_token_materialization_admission_v1`。
- `token_plan_identity` 由 input、generated part、hygiene、trace、generated item declaration、declared generated
  name 和 token stream name 决定。
- `token_buffer_identity` 由 input、generated part、token plan、hygiene、source-map identity、token buffer kind
  和 token stream name 决定。
- `compiler_owned = true`。
- `admitted = true`。
- `generated_source_text = false`。
- `parse_ready = false`。
- `external_process_required = false`。
- `standard_library_required = false`。
- `runtime_required = false`。
- `produced_user_generated_code = false`。

M21i 改变的是 `materialized_tokens` 的合法状态：

- 对 `derive` 输入：`materialized_tokens = true`，`blocker_reason` 为
  `compiler-owned derive token prototype remains parser-blocked in M21i`。
- 对非 `derive` 输入：`materialized_tokens = false`，`blocker_reason` 为
  `non-derive item attribute token materialization remains blocked in M21i`。

这里的 `materialized_tokens = true` 只表示 compiler 已产生结构化 prototype token records；这些 records 不是
generated source text，不可被 parser 消费，也不进入 sema lookup/export。

## GeneratedTokenBufferStub

每个 `GeneratedTokenBufferStub` 和一个 `EarlyItemMacroInput` 一一对应。M21i 支持两种合法 buffer shape。

### 非 derive 空 buffer

非 `derive` item attribute 继续保持空 buffer：

- `token_buffer_kind = compiler_owned_empty_token_stream`。
- `token_producer_policy = compiler_owned_blocked_empty_token_producer_v1`。
- `blocker_reason = non-derive generated token buffer remains empty and parser-blocked in M21i`。
- `token_count = 0`。
- `empty = true`。
- `materialized_tokens = false`。
- `generated_source_text = false`。
- `parser_consumable = false`。
- `produced_user_generated_code = false`。

### derive prototype buffer

`derive` item attribute 会得到 compiler-owned prototype buffer：

- `token_buffer_kind = compiler_owned_builtin_derive_token_stream_prototype`。
- `token_producer_policy = compiler_owned_builtin_derive_token_producer_prototype_v1`。
- `blocker_reason = compiler-owned generated token buffer remains parser-blocked in M21i`。
- `token_count = derive attribute token_tree token count + 2`。
- `empty = false`。
- `materialized_tokens = true`。
- `generated_source_text = false`。
- `parser_consumable = false`。
- `produced_user_generated_code = false`。

`materialization_identity` 会混入 input identity、generated part、admission token plan、admission token buffer、
source-map identity、hygiene mark、token buffer kind、token producer policy 和 token count。它用于区分“已有
buffer identity”与“本次 compiler-owned token record materialization 事实”。

## GeneratedTokenRecord

`GeneratedTokenRecord` 是 M21i 的新增事实，对应 future `macro_generated_token_record_fact`。每条 record 都包含：

- `item`
- `module`
- `part_index`
- `attribute_index`
- `token_index`
- `token_buffer_identity`
- `token_identity`
- `source_map_identity`
- `hygiene_mark`
- `kind`
- `text`
- `token_role`
- `anchor_range`
- `compiler_owned`
- `parser_visible`
- `produced_user_generated_code`

M21i 只为 `derive` 输入产生 records。record 序列固定为：

1. begin sentinel：
   - `kind = identifier`
   - `text = __aurex_builtin_derive_begin`
   - `token_role = derive_codegen_begin`
   - `anchor_range = attribute_range`
2. 每个源 attribute token tree entry 对应一个 source placeholder：
   - `kind = 原 attribute token kind`
   - `text = __aurex_builtin_derive_source_token_<token_index>`
   - `token_role = derive_source_token_placeholder`
   - `anchor_range = 原 attribute token range`
3. end sentinel：
   - `kind = identifier`
   - `text = __aurex_builtin_derive_end`
   - `token_role = derive_codegen_end`
   - `anchor_range = attribute_range`

这些 placeholder text 是 compiler-owned internal spelling，不复制用户 token text，也不伪装成真实 macro output。
原始 `TokenKind` 和 source range 只用于 provenance/debugging。所有 record 必须满足：

- `compiler_owned = true`
- `parser_visible = false`
- `produced_user_generated_code = false`

## 与 M21a 设计事实的关系

M21a design gate 要求后续宏系统具备：

- `macro_token_materialization_fact`
- `macro_generated_token_buffer_fact`
- `macro_generated_token_record_fact`
- `macro_generated_part_parse_fact`
- `macro_hygiene_mark_fact`
- `macro_expansion_origin_fact`
- `macro_expansion_trace_fact`
- `macro_generated_source_map_fact`
- `macro_declared_name_fact`

M21i 把 M21h 的 empty generated token buffer stub 扩展为第一版 compiler-owned token record facts：

- `TokenMaterializationAdmissionStub` 对应 future `macro_token_materialization_fact` 的准入层。
- `GeneratedTokenBufferStub` 对应 future `macro_generated_token_buffer_fact` 的 buffer 层。
- `GeneratedTokenRecord` 对应 future `macro_generated_token_record_fact` 的 token record 层。
- `macro_generated_part_parse_fact` 仍由 M21e 的 `GeneratedModulePartParseMergeStub` 承担。
- `declared_name_set`、`hygiene_mark`、`trace_identity` 和 `source_map_identity` 继续消费 M21f facts。
- `declaration_identity`、`generated_item_key` 和 `declared_name_identity` 继续消费 M21g facts。

这些事实仍是 prototype，不是完整语义执行结果。

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
- generated token records。
- summary。
- M21c plan fingerprint。

因此修改 token producer policy、token count、materialization identity、record text、record role、record kind、
record source-map identity、record hygiene mark、parser-visible flag 或 user-generated-code flag 都会改变 expansion
fingerprint，并且 validation 会拒绝不能按 input / generated part / hygiene / trace / generated item / declared
generated name / admission / buffer 重新计算的漂移。

`dump_early_item_expansion()` 现在会输出：

- `token_materialization_admission_stub` 行。
- `generated_token_buffer_stub` 行。
- `producer=compiler_owned_builtin_derive_token_producer_prototype_v1` 或
  `producer=compiler_owned_blocked_empty_token_producer_v1`。
- `materialization_identity`。
- `generated_token_record` 行。
- token kind、internal text、role、compiler-owned flag、parser-visible flag、token identity、token buffer identity、
  source-map identity 和 hygiene mark。

## Validation

M21i validation 会拒绝以下漂移：

- expansion result 名称不是 `M21i Compiler-Owned Generated Token Buffer Prototype`。
- plan 不是有效 M21c plan。
- token materialization admissions 或 generated token buffers 与 inputs 数量不一致。
- derive admission 没有 `materialized_tokens = true`。
- 非 derive admission 错误声称 `materialized_tokens = true`。
- admission 声称 generated source text、parse-ready、需要 external process、需要标准库、需要 runtime 或 produced
  user code。
- generated token buffer 与 input、generated part、hygiene、trace 或 admission 不一致。
- derive buffer 不是 `compiler_owned_builtin_derive_token_stream_prototype`。
- derive buffer 不是 `compiler_owned_builtin_derive_token_producer_prototype_v1`。
- derive buffer token_count 不是 `attribute.token_tree.size() + 2`。
- derive buffer 错误声称 empty、parser-consumable、generated source text 或 produced user code。
- 非 derive buffer 不是 `compiler_owned_empty_token_stream`。
- 非 derive buffer 不是 `compiler_owned_blocked_empty_token_producer_v1`。
- 非 derive buffer token_count 非 0，或 empty 不是 true。
- `materialization_identity` 为空或无法重算。
- generated token records 数量与 derive buffers 的 token_count 不一致。
- generated token record 不是 compiler-owned。
- generated token record 变成 parser-visible。
- generated token record 声称 produced user code。
- begin/end sentinel 的 kind、text、role 或 anchor range 不符合 M21i 规则。
- source placeholder 的 role、internal text、kind、source range 或 identity 不符合 M21i 规则。
- generated parse / merge stub、hygiene、trace、source map、generated item declaration、declared generated name、
  summary 或 fingerprint 出现 M21e/M21f/M21g/M21h 已固定的 drift。

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
- 每个 macro input 有 deterministic generated token buffer stub。
- `derive` input 有 compiler-owned generated token record facts。
- 非 `derive` input 的 generated token buffer 仍为空。
- compiler 内部可以 dump / summarize future generated item identity、declared name identity、token plan identity、
  token buffer identity、materialization identity 和 generated token record identities。
- validation 可以阻止 generated tokens 被误标为 parser-visible 或 user-generated code。

当前用户可见宏能力仍只有内建 `#[derive(Copy, Eq, Hash)]` capability。M21i 仍不生成用户代码。

## 仍不实现

M21i 仍不实现：

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
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## 下一步

下一步建议继续 M21j：在保持 parser-blocked / no user-code 的前提下，把 compiler-owned token buffer admission
gate 和 generated module part parse admission gate 明确分开。M21j 应优先固定“什么条件下 generated token buffer
才允许进入 parser”的结构化 guard、诊断和 dump，而不是直接打开 parser consumption。这样可以继续复用 M21e/M21f/M21g/M21h/M21i
facts，并避免把 token prototype 误当成真实用户宏展开。

该建议已由 [Aurex M21j Generated Token Parser Admission Gate](m21j-generated-token-parser-admission-gate.md)
承接；M21i 文档保留为 token buffer prototype 的历史阶段说明。

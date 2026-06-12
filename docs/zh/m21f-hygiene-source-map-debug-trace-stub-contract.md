# Aurex M21f Hygiene Source Map Debug Trace Stub Contract

日期：2026-06-12
阶段：M21f Hygiene Source Map Debug Trace Stub Contract
状态：hygiene/source-map/debug-trace stub facts / no user-generated code

## 目标

M21f 继续沿用 M21d/M21e 已接入真实 frontend pipeline 的 `macro.expand_items` 边界，不新增 pipeline stage，
也不让宏输出绕过 parser、sema、borrow checker、visibility、trait solver 或 IR verifier。本阶段固定的是
宏展开后续必须携带的 hygiene、source map 和 debug trace 事实形状，而不是实现真实宏代码生成。

核心目标：

- 每个 parsed item attribute 仍有 `EarlyItemMacroInput`。
- 每个 macro input 仍有 `ExpansionSourceMapPlaceholder`，并继续保持 `real_source_map=false`。
- 每个 macro input 新增一个 `ExpansionHygieneStub`。
- 每个 macro input 新增一个 `ExpansionTraceStub`。
- stub 固定 `origin_mark_hygiene_v1` 和 `expansion_source_map_debug_trace_v1` 两个策略名。
- stub 只建立稳定身份、origin、mark、diagnostic anchor 和 blocker，不解析、不 merge、不生成用户代码。
- summary、dump、fingerprint 和 validation 都能发现 hygiene/source-map/debug-trace 契约漂移。

## 已实现

新增 frontend macro API：

- `frontend::macro::ExpansionHygieneStub`
- `frontend::macro::ExpansionTraceStub`
- `is_valid(const ExpansionHygieneStub&)`
- `is_valid(const ExpansionTraceStub&)`

`EarlyItemExpansionResult` 新增：

- `hygiene_stubs`
- `trace_stubs`

`EarlyItemExpansionSummary` 新增：

- `hygiene_stub_count`
- `unresolved_hygiene_stub_count`
- `declared_name_stub_count`
- `call_site_capture_count`
- `trace_stub_count`
- `real_source_map_count`
- `debug_trace_available_count`
- `cli_emit_expanded_available_count`

M21f result 名称固定为：

```text
M21f Hygiene Source Map Debug Trace Stub Contract
```

## Pipeline 边界

M21f 仍运行在同一个 driver profile stage：

```text
module.read / module.lex / module.parse / module.append
macro.expand_items
ast.dump 或 sema.analyze
```

这点保持不变：M21f 不是第二个宏展开 pass，也不是 parser 里的特殊路径。它只是把 `EarlyItemExpansionResult`
继续扩展为带 hygiene/source-map/debug-trace stub contract 的 result。后续真实宏展开仍必须沿这个边界推进，
不能把生成代码直接塞进 sema 或 IR lowering。

## ExpansionHygieneStub

每个 `ExpansionHygieneStub` 和一个 `EarlyItemMacroInput` 一一对应。当前合法 stub 必须满足：

- `item`、`module`、`part_index`、`attribute_index` 与 input 一致。
- `attached_part` 与 input 一致。
- `expansion_origin` 等于 input 的 query-key fingerprint。
- `call_site_mark` 非空，并由 input identity、attached part、query-key fingerprint 和 token-tree fingerprint 共同决定。
- `definition_site_mark` 非空，并与 call-site mark 使用不同 marker 生成。
- `generated_fresh_mark` 非空，并与 call-site / definition-site mark 分离。
- `declared_name_set` 非空，作为 future declared generated names 的稳定身份。
- `policy = origin_mark_hygiene_v1`。
- `resolved = false`。
- `declared_names_visible = false`。
- `captures_call_site_locals = false`。

M21f 的 hygiene 只是结构化 stub。它还不执行真实 name resolution，不把 declared generated names 暴露给 lookup，
也不允许 generated identifiers 捕获 call-site locals。

## ExpansionTraceStub

每个 `ExpansionTraceStub` 和一个 `EarlyItemMacroInput` 一一对应。当前合法 stub 必须满足：

- `item`、`module`、`part_index`、`attribute_index` 与 input 一致。
- `attached_part` 与 input 一致。
- `attribute_range` 和 `token_tree_range` 与 input 一致。
- `expansion_origin` 等于 input 的 query-key fingerprint。
- `trace_identity` 非空，作为 future expansion trace 的稳定身份。
- `generated_source_map_identity` 非空，作为 future generated source map 的稳定身份。
- `diagnostic_anchor` 非空，作为 future diagnostic expansion stack 的锚点。
- `trace_policy = expansion_source_map_debug_trace_v1`。
- `blocker_reason` 固定为 real macro source map/debug trace 在 M21f 被阻塞。
- `real_source_map = false`。
- `debug_trace_available = false`。
- `cli_emit_expanded_available = false`。

M21f 只让 diagnostics/tooling/cache 能看见“未来必须有 trace”的身份边界；它不提供真实 expanded text、
真实 generated source map、debug trace CLI 或 `--emit-expanded`。

## 与 M21a 设计事实的关系

M21a design gate 要求后续宏系统具备：

- `macro_hygiene_mark_fact`
- `macro_origin_id_fact`
- `macro_declared_name_fact`
- `macro_expansion_origin_fact`
- `macro_expansion_trace_fact`
- `macro_generated_source_map_fact`

M21f 把这些要求落成 early item expansion result 上的结构化 stub：

- `ExpansionHygieneStub::call_site_mark`、`definition_site_mark`、`generated_fresh_mark` 对应
  `macro_hygiene_mark_fact`。
- `ExpansionHygieneStub::declared_name_set` 对应 future `macro_declared_name_fact` 的稳定身份。
- `ExpansionTraceStub::expansion_origin`、`trace_identity`、`generated_source_map_identity` 和
  `diagnostic_anchor` 对应 `macro_expansion_origin_fact`、`macro_expansion_trace_fact` 和
  `macro_generated_source_map_fact`。

这些事实仍是 stub，不是完整语义执行结果。

## Fingerprint 与 Dump

`early_item_expansion_fingerprint()` 现在混入：

- inputs。
- generated placeholders。
- generated parse / merge stubs。
- source-map placeholders。
- hygiene stubs。
- trace stubs。
- summary。
- M21c plan fingerprint。

因此修改 hygiene mark、declared name set、trace identity、generated source map identity、diagnostic anchor、
policy、blocker 或 no-op flags 都会改变 expansion fingerprint。

`dump_early_item_expansion()` 现在会输出：

- `hygiene_stub` 行。
- `policy=origin_mark_hygiene_v1`。
- call-site / definition-site / fresh mark。
- declared name set identity。
- `trace_stub` 行。
- `policy=expansion_source_map_debug_trace_v1`。
- trace identity。
- generated source map identity。
- diagnostic anchor。
- blocker reason。

## Validation

M21f validation 会拒绝以下漂移：

- expansion result 名称不是 `M21f Hygiene Source Map Debug Trace Stub Contract`。
- plan 不是有效 M21c plan。
- source-map placeholders、hygiene stubs 或 trace stubs 与 inputs 数量不一致。
- source-map placeholder 与 input 的 item/module/attribute/range/origin 不一致。
- hygiene stub 与 input 的 item/module/part/attribute/attached part/origin 不一致。
- hygiene mark、declared name set 为空或无法按 input 重算。
- hygiene policy 不是 `origin_mark_hygiene_v1`。
- hygiene stub 声称已经 resolved、declared names visible 或 captures call-site locals。
- trace stub 与 input 的 item/module/part/attribute/attached part/range/origin 不一致。
- trace identity、generated source map identity 或 diagnostic anchor 为空或无法按 input 重算。
- trace policy 不是 `expansion_source_map_debug_trace_v1`。
- trace blocker reason 不是 M21f 固定 blocker。
- source-map placeholder 或 trace stub 声称已有 real source map。
- source-map placeholder 或 trace stub 声称已有 debug trace。
- trace stub 声称 `--emit-expanded` 可用。
- generated parse / merge stub、summary 或 fingerprint 出现 M21e 已固定的 drift。

## 当前能做什么

当前内部可用能力：

- parser 仍能保存通用 item attribute token tree。
- `macro.expand_items` 会收集每个 parsed item attribute 的 token-tree fingerprint 和 query-key fingerprint。
- `derive` attribute 仍标记为 `builtin_derive_passthrough`，兼容内建 derive path。
- 非 `derive` item attribute 仍标记为 `blocked_unimplemented_attribute`。
- 每个带 attribute 的 source module part 有 deterministic generated placeholder。
- 每个 generated placeholder 有 deterministic parse / merge stub。
- 每个 macro input 有 deterministic source-map placeholder、hygiene stub 和 trace stub。
- compiler 内部可以 dump / summarize hygiene mark identity、declared name set identity、trace identity、
  generated source map identity 和 diagnostic anchor。
- tests 可以验证 hygiene/source-map/debug-trace stub contract 和 no-op 边界。

当前用户可见宏能力仍只有内建 `#[derive(Copy, Eq, Hash)]`。M21f 仍不生成用户代码。

## 非目标

M21f 仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## 与后续阶段的关系

M21f 让后续阶段可以继续在同一个 result 结构上前进：

- 后续 compiler-owned derive / attached item codegen 可以消费 `declared_name_set` 身份，但仍必须先声明 generated names。
- 后续 generated module part parse 可把 M21e 的 parse blocker 打开，但仍必须先经过 parser。
- 后续 generated module part merge 可继续使用 `merge_ordering_key` 做 deterministic merge。
- 后续真实 diagnostics 可以把 `diagnostic_anchor` 扩展成 expansion stack。
- 后续 `--emit-expanded` 必须先有真实 generated source map 和 stable trace，不能只靠字符串拼接。
- external procedural macro 必须继续等待 sandbox、manifest、permission、implementation fingerprint 和 debug trace
  边界，不应直接跳到执行。

所有后续阶段仍应保持：宏输出必须走普通语言规则，不能绕过 parser、sema、borrow checker、visibility、
trait solver、IR verifier 和 normal diagnostics。

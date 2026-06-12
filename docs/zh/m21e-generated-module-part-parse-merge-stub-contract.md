# Aurex M21e Generated Module Part Parse/Merge Stub Contract

日期：2026-06-12
阶段：M21e Generated Module Part Parse/Merge Stub Contract
状态：generated module part buffer identity / parse-merge lifecycle stub / no user-generated code

## 目标

M21e 继续沿用 M21d 已接入真实 frontend pipeline 的 `macro.expand_items` 边界，不新增 pipeline stage，也不让
宏输出绕过 parser、sema、borrow checker 或 IR verifier。本阶段固定的是 generated module part 后续进入
parse / merge 的契约，而不是实现真实代码生成。

核心目标：

- 每个带 attribute 的 source module part 仍只产生一个 deterministic generated module part placeholder。
- 每个 generated placeholder 现在同时有一个 `GeneratedModulePartParseMergeStub`。
- stub 固定 generated buffer identity、parse config fingerprint、merge ordering key 和 expansion origin。
- stub 明确处于 parse / merge blocked 状态，仍不生成用户代码。
- summary、dump、fingerprint 和 validation 都能发现 parse / merge 契约漂移。

## 已实现

新增 frontend macro API：

- `frontend::macro::GeneratedModulePartParseMergeStub`
- `frontend::macro::GeneratedModulePartLifecycleState`
- `generated_module_part_lifecycle_state_name()`
- `is_valid(GeneratedModulePartLifecycleState)`
- `is_valid(const GeneratedModulePartParseMergeStub&)`

`EarlyItemExpansionResult` 新增：

- `generated_part_stubs`

`EarlyItemExpansionSummary` 新增：

- `generated_part_stub_count`
- `materialized_buffer_stub_count`
- `parse_blocked_count`
- `merge_blocked_count`
- `sema_visible_generated_part_count`

M21e result 名称固定为：

```text
M21e Generated Module Part Parse/Merge Stub Contract
```

## Pipeline 边界

M21e 仍运行在同一个 driver profile stage：

```text
module.read / module.lex / module.parse / module.append
macro.expand_items
ast.dump 或 sema.analyze
```

这点很重要：M21e 不是第二个宏展开 pass，也不是 parser 里的特殊路径。它只是把 M21d 的 no-op expansion result
扩展成带 parse / merge stub contract 的 result。后续真实 generated module part parse / merge 必须沿同一个
`EarlyItemExpansionResult` 边界推进。

## GeneratedModulePartParseMergeStub

每个 `GeneratedModulePartParseMergeStub` 和一个 `GeneratedModulePartPlaceholder` 一一对应。当前合法 stub 必须满足：

- `module`、`source_part_index`、`generated_stable_index` 与 placeholder 一致。
- `source_part` 指向原始 source module part。
- `generated_part` 指向 `SourceRole::generated` / `ModulePartKind::generated` generated module part。
- `generated_buffer_identity` 非空，并由 source part、generated part、module、source part index、generated stable index
  和 generated buffer name 共同决定。
- `parse_config_fingerprint` 非空，并固定到当前默认 parser config 和 generated part identity。
- `merge_ordering_key` 非空，并固定 generated part 在 future merge 序列里的稳定排序身份。
- `expansion_origin` 等于 placeholder 的 output fingerprint。
- `generated_buffer_name` 使用 deterministic `m21e-noop-generated-buffer:<module>:<generated_stable_index>` 格式。
- `blocker_reason` 固定为 generated module part parse / merge 在 M21e 被阻塞。
- `lifecycle_state = GeneratedModulePartLifecycleState::merge_blocked`。
- `materialized_buffer = true`，表示 buffer identity stub 已经存在。
- `parsed = false`。
- `merged = false`。
- `sema_visible = false`。
- `produced_user_generated_code = false`。

## Lifecycle 模型

`GeneratedModulePartLifecycleState` 当前公开四个状态名：

- `planned`
- `materialized_buffer_stub`
- `parse_blocked`
- `merge_blocked`

M21e 的合法 expansion result 只允许 `merge_blocked`。其他枚举值是为了把后续生命周期说清楚，并让 tests /
dump / validation 先固定名字；它们不是当前可用行为。

生命周期含义：

- `planned`：后续阶段可用于“只有 generated part identity，还没有 buffer stub”的状态。
- `materialized_buffer_stub`：后续阶段可用于“buffer identity 已建立，但还没有进入 parser”的状态。
- `parse_blocked`：后续阶段可用于“buffer 已可见，但 parse 被 gate 阻断”的状态。
- `merge_blocked`：M21e 当前唯一合法状态，表示 generated buffer identity 已存在，但 parse 和 merge 都不能发生。

## Fingerprint 与 Dump

`early_item_expansion_fingerprint()` 现在混入：

- inputs。
- generated placeholders。
- generated parse / merge stubs。
- source-map placeholders。
- summary。
- M21c plan fingerprint。

因此修改 `generated_buffer_identity`、`parse_config_fingerprint`、`merge_ordering_key`、`expansion_origin`、
`generated_buffer_name`、`blocker_reason` 或 lifecycle flags 都会改变 expansion fingerprint。

`dump_early_item_expansion()` 现在会输出 `parse_merge_stub` 行，并显示：

- lifecycle。
- buffer name。
- materialized / parsed / merged / sema-visible 状态。
- blocker reason。
- buffer identity。
- parse config fingerprint。
- merge ordering key。

## Validation

M21e validation 会拒绝以下漂移：

- expansion result 名称不是 `M21e Generated Module Part Parse/Merge Stub Contract`。
- plan 不是有效 M21c plan。
- generated placeholder 不是 `SourceRole::generated` / `ModulePartKind::generated`。
- generated placeholder 声称已经 parsed、merged 或 produced user-generated code。
- `generated_part_stubs` 与 `generated_parts` 数量不一致。
- stub 与 placeholder 的 module、source part、generated part、stable index 或 expansion origin 不一致。
- stub 的 generated buffer name、generated buffer identity、parse config fingerprint 或 merge ordering key 与
  placeholder 重新计算结果不一致。
- stub lifecycle 不是 `merge_blocked`。
- stub 声称没有 materialized buffer identity。
- stub 声称已经 parsed、merged、sema-visible 或 produced user-generated code。
- source-map placeholder 声称已有 real source map 或 debug trace。
- summary 和 result 不一致。
- fingerprint 和 result 不一致。

## 当前能做什么

当前内部可用能力：

- parser 仍能保存通用 item attribute token tree。
- `macro.expand_items` 会收集每个 parsed item attribute 的 token-tree fingerprint 和 query-key fingerprint。
- `derive` attribute 仍标记为 `builtin_derive_passthrough`，兼容内建 derive path。
- 非 `derive` item attribute 仍标记为 `blocked_unimplemented_attribute`。
- 每个带 attribute 的 source module part 有 deterministic generated placeholder。
- 每个 generated placeholder 有 deterministic parse / merge stub。
- compiler 内部可以 dump / summarize generated module part 的 future parse / merge identity。
- tests 可以验证 generated buffer identity、parse config、merge ordering、lifecycle blocker 和 no-op 边界。

当前用户可见宏能力仍只有内建 `#[derive(Copy, Eq, Hash)]`。M21e 仍不生成用户代码。

## 非目标

M21e 仍不实现：

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
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## 与后续阶段的关系

M21e 让后续阶段可以继续在同一个 result 结构上前进：

- 后续 hygiene / source-map / debug trace 可把 expansion origin 扩展成真实可诊断数据。
- 后续 generated module part parse 可把 `parse_blocked` 打开，但仍必须先经过 parser。
- 后续 generated module part merge 可使用 `merge_ordering_key` 做 deterministic merge。
- 第一条真实代码生成主线仍应优先做 compiler-owned derive / attached item codegen。
- external procedural macro 必须继续等待 sandbox、manifest、permission、implementation fingerprint 和 debug trace
  边界，不应直接跳到执行。

所有后续阶段仍应保持：宏输出必须走普通语言规则，不能绕过 parser、sema、borrow checker、visibility、
trait solver、IR verifier 和 normal diagnostics。

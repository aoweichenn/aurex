# Aurex M21n Parser Consumption Contract Gate

阶段：M21n Parser Consumption Contract Gate

## 目标

M21n 在 M21m preflight 之后增加 generated module part 级 parser consumption contract gate。它回答的问题是：
如果未来某个阶段要把 generated token buffer 交给 parser，那么一个 generated part 在进入 parser 之前必须满足
哪些合同条件。

M21n 仍然不打开 parser consumption。它只是把 M21m 的 per-input preflight facts 按 generated module part 汇总，
并形成可查询、可缓存、可校验的 gate surface。

## 新增 API

`EarlyItemExpansionResult` 新增：

```cpp
std::vector<GeneratedTokenParserConsumptionContractGate> parser_consumption_contract_gates;
```

每个 generated module part 对应一个 `GeneratedTokenParserConsumptionContractGate`。gate 绑定：

- generated part identity：`module`、`source_part_index`、`attached_part`、`generated_part`。
- M21e parser buffer facts：`generated_buffer_identity`、`parse_config_fingerprint`。
- M21l report fact：`report_identity`。
- M21n identities：`contract_identity`、`contract_grouping_identity`、`contract_anchor_identity`。
- query name：`m21n-parser-consumption-contract:<module>:<part>`。

## Contract 内容

M21n gate 当前记录：

- `preflight_entry_count`。
- `blocked_entry_count`。
- `derive_entry_count`。
- `empty_entry_count`。
- `contiguous_index_entry_count`。
- `delimiter_balanced_entry_count`。
- `source_anchor_covered_entry_count`。
- `parse_config_compatible_entry_count`。
- `diagnostic_projection_entry_count`。

当前 `all_entries_structurally_checked = true` 的含义是：该 generated part 下所有 M21m preflight entries 都通过了
结构性审计。它不代表 parser 已经可以消费这些 token。

## 仍然阻塞

M21n gate 固定：

- `contract_policy = generated_token_parser_consumption_contract_gate_v1`。
- `blocked_reason = parser consumption contract remains parser-blocked in M21n`。
- `query_reusable = true`。
- `contract_visible = true`。
- `parser_admitted = false`。
- `parse_ready = false`。
- `parser_consumable = false`。
- `generated_part_parsed = false`。
- `generated_part_merged = false`。
- `sema_visible = false`。
- `emit_expanded_available = false`。
- `debug_trace_available = false`。
- `source_map_available = false`。
- `produced_user_generated_code = false`。

M21n 仍不实现：

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
- sema visibility for generated items。
- `--emit-expanded`。

## Validation

M21n validation 会拒绝以下漂移：

- 缺失 `parser_consumption_contract_gates`。
- gate 与 generated part、M21e parse/merge stub、M21l report 或 M21m preflight group 不能重算。
- `contract_identity`、`contract_grouping_identity`、`contract_anchor_identity` 漂移。
- query name 或 policy 漂移。
- entry totals 与 M21m preflight entries 不一致。
- `all_entries_structurally_checked` 与实际 totals 不一致。
- parser/debug/source-map/emit/sema/user-code flags 被打开。

## 测试

当前测试覆盖：

- generated part 级 contract gate positive facts。
- contract identity、counts、visibility、parser-consumable、emit-expanded、source-map 和 user-code negative matrix。
- summary、dump 和 fingerprint 对 contract gate facts 的响应。

## 下一步

M21n 的输出由 M21o release closure 汇总。M21o 用一个 closure report 把 M21m preflight 和 M21n contract gate
作为 M21 宏展开边界收口的一部分固定下来。
